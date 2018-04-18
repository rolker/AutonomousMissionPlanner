#include "roslink.h"
#include "std_msgs/Bool.h"
#include "std_msgs/String.h"
#include <QDebug>
#include <QPainter>
#include <QGraphicsSvgItem>
#include "autonomousvehicleproject.h"
#include "backgroundraster.h"
#include <QTimer>
#include "gz4d_geo.h"

ROSLink::ROSLink(AutonomousVehicleProject* parent): QObject(parent), GeoGraphicsItem(),m_node(nullptr), m_spinner(nullptr),m_have_local_reference(false),m_heading(0.0), m_active(false),m_helmMode("standby")
{
    setAcceptHoverEvents(false);
    setOpacity(1.0);
    setFlag(QGraphicsItem::ItemIsMovable, false);

    //QGraphicsSvgItem *symbol = new QGraphicsSvgItem(this);
    //symbol->setSharedRenderer(autonomousVehicleProject()->symbols());
    //symbol->setElementId("Vessel");
    //symbol->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    
    qRegisterMetaType<QGeoCoordinate>();
    //connectROS();
}

void ROSLink::connectROS()
{    
    if(ros::master::check())
    {
        if(!m_node)
        {
            m_node = new ros::NodeHandle;
            m_spinner = new ros::AsyncSpinner(0);
            m_geopoint_subscriber = m_node->subscribe("/udp/position", 10, &ROSLink::geoPointStampedCallback, this);
            m_origin_subscriber = m_node->subscribe("/udp/origin", 10, &ROSLink::originCallback, this);
            m_heading_subscriber = m_node->subscribe("/udp/heading", 10, &ROSLink::headingCallback, this);
            m_ais_subscriber = m_node->subscribe("/udp/ais", 10, &ROSLink::aisCallback, this);
            m_active_publisher = m_node->advertise<std_msgs::Bool>("/udp/active",1);
            m_helmMode_publisher = m_node->advertise<std_msgs::String>("/udp/helm_mode",1);
            m_wpt_updates_publisher = m_node->advertise<std_msgs::String>("/udp/wpt_updates",1);
            m_loiter_updates_publisher = m_node->advertise<std_msgs::String>("/udp/loiter_updates",1);
            m_spinner->start();
            emit rosConnected(true);
        }
    }
    else
    {
        if(m_node)
        {
            delete m_node;
            m_node = nullptr;
            delete m_spinner;
            m_spinner = nullptr;
            emit rosConnected(false);
        }
    }
    QTimer::singleShot(1000,this,SLOT(connectROS()));
}

QRectF ROSLink::boundingRect() const
{
    return shape().boundingRect();
}

void ROSLink::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    painter->save();

    QPen p;
    if(m_node)
        p.setColor(Qt::darkGreen);
    else
        p.setColor(Qt::darkRed);
    p.setCosmetic(true);
    p.setWidth(3);
    painter->setPen(p);
    painter->drawPath(shape());

    painter->restore();
  
    
}

QPainterPath ROSLink::shape() const
{
    if (m_local_location_history.size() > 1)
    {
        QPainterPath ret;
        auto p = m_local_location_history.begin();
        ret.moveTo(*p);
        p++;
        while(p != m_local_location_history.end())
        {
            ret.lineTo(*p);
            p++;
        }
        auto last = *(m_local_location_history.rbegin());
        
        drawTriangle(ret,last,m_heading,10);
        
        ret.addRect(-1,-5,2,10);
        ret.addRect(-5,-1,10,2);

        for(auto contactList: m_contacts)
        {
            auto p = contactList.second.begin();
            ret.moveTo(p->location_local);
            p++;
            while(p != contactList.second.end())
            {
                ret.lineTo(p->location_local);
                p++;
            }
            auto last = *(contactList.second.rbegin());
            
            drawTriangle(ret,last.location_local,last.heading,10);
        }
        
        return ret;
    }
    return QPainterPath();
}

void ROSLink::drawTriangle(QPainterPath& path, const QPointF& location, double heading_degrees, double scale) const
{
        double heading_radians = heading_degrees*2*M_PI/360.0;
        double sin_heading = sin(heading_radians);
        double cos_heading = cos(heading_radians);
        
        path.moveTo(location.x()+(-scale*.5*cos_heading)-( scale*.5*sin_heading),location.y()+(-scale*.5*sin_heading)+( scale*.5*cos_heading));
        path.lineTo(location.x()                        -(-scale   *sin_heading),location.y()                        +(-scale*   cos_heading));
        path.lineTo(location.x()+( scale*.5*cos_heading)-( scale*.5*sin_heading),location.y()+( scale*.5*sin_heading)+( scale*.5*cos_heading));
        path.lineTo(location.x()+(-scale*.5*cos_heading)-( scale*.5*sin_heading),location.y()+(-scale*.5*sin_heading)+( scale*.5*cos_heading));
}


void ROSLink::read(const QJsonObject& json)
{
}

void ROSLink::write(QJsonObject& json) const
{
}

void ROSLink::geoPointStampedCallback(const geographic_msgs::GeoPointStamped::ConstPtr& message)
{
    QGeoCoordinate position(message->position.latitude,message->position.longitude,message->position.altitude);
    QMetaObject::invokeMethod(this,"updateLocation", Qt::QueuedConnection, Q_ARG(QGeoCoordinate, position));
}

void ROSLink::originCallback(const geographic_msgs::GeoPoint::ConstPtr& message)
{
    m_origin.setLatitude(message->latitude);
    m_origin.setLongitude(message->longitude);
    m_origin.setAltitude(message->altitude);
    QMetaObject::invokeMethod(this,"updateOriginLocation", Qt::QueuedConnection, Q_ARG(QGeoCoordinate, m_origin));
    //qDebug() << m_origin;
}

void ROSLink::headingCallback(const mission_plan::NavEulerStamped::ConstPtr& message)
{
    QMetaObject::invokeMethod(this,"updateHeading", Qt::QueuedConnection, Q_ARG(double, message->orientation.heading));
}

void ROSLink::aisCallback(const asv_msgs::AISContact::ConstPtr& message)
{
    qDebug() << message->mmsi << ": " << message->name.c_str();
    Contact c;
    c.name = message->name;
    c.location.setLatitude(message->position.latitude);
    c.location.setLongitude(message->position.longitude);
    if(c.heading == -1)
        c.heading = message->cog*180.0/M_PI;
    else
        c.heading = message->heading*180.0/M_PI;
    QMetaObject::invokeMethod(this,"addAISContact", Qt::QueuedConnection, Q_ARG(uint32_t, message->mmsi),Q_ARG(Contact, c));
}


void ROSLink::sendWaypoints(const QList<QGeoCoordinate>& waypoints)
{
    qDebug() << "origin: " << m_origin;
    gz4d::geo::Point<double,gz4d::geo::WGS84::LatLon> gr(m_origin.latitude(),m_origin.longitude(),m_origin.altitude());
    qDebug() << "as gz4d::geo::Point: " << gr[0] << ", " << gr[1] << ", " << gr[2];
    gz4d::geo::LocalENU<> geoReference(gr);    //geoReference = gz4d::geo::LocalENU<>(gr);

    std::stringstream updates;
    updates << "points = ";
    for(auto wp: waypoints)
    {
        qDebug() << "wp: " << wp;
        gz4d::geo::Point<double,gz4d::geo::WGS84::LatLon> ggp(wp.latitude(),wp.longitude(),0.0);
        qDebug() << "ggp: " << ggp[0] << ", " << ggp[1] << ", " << ggp[2];
        gz4d::geo::Point<double,gz4d::geo::WGS84::ECEF> ecef(ggp);
        qDebug() << "ecef: " << ecef[0] << ", " << ecef[1] << ", " << ecef[2];
        gz4d::Point<double> position = geoReference.toLocal(ecef);
        updates << position[0] << ", " << position[1] << ":";
    }
    
    std_msgs::String rosUpdates;
    rosUpdates.data = updates.str();
    m_wpt_updates_publisher.publish(rosUpdates);
}

void ROSLink::sendLoiter(const QGeoCoordinate& loiterLocation)
{
    qDebug() << "origin: " << m_origin;
    gz4d::geo::Point<double,gz4d::geo::WGS84::LatLon> gr(m_origin.latitude(),m_origin.longitude(),m_origin.altitude());
    qDebug() << "as gz4d::geo::Point: " << gr[0] << ", " << gr[1] << ", " << gr[2];
    gz4d::geo::LocalENU<> geoReference(gr);    //geoReference = gz4d::geo::LocalENU<>(gr);

    std::stringstream updates;
    updates << "center_assign = ";

    qDebug() << "loiterLocation: " << loiterLocation;
    gz4d::geo::Point<double,gz4d::geo::WGS84::LatLon> ggp(loiterLocation.latitude(),loiterLocation.longitude(),0.0);
    qDebug() << "ggp: " << ggp[0] << ", " << ggp[1] << ", " << ggp[2];
    gz4d::geo::Point<double,gz4d::geo::WGS84::ECEF> ecef(ggp);
    qDebug() << "ecef: " << ecef[0] << ", " << ecef[1] << ", " << ecef[2];
    gz4d::Point<double> position = geoReference.toLocal(ecef);
    updates << position[0] << ", " << position[1] << ":";
    
    std_msgs::String rosUpdates;
    rosUpdates.data = updates.str();
    m_loiter_updates_publisher.publish(rosUpdates);
}

void ROSLink::updateLocation(const QGeoCoordinate& location)
{
    if(m_have_local_reference)
    {
        m_location_history.push_back(location);
        m_local_location_history.push_back(geoToPixel(location,autonomousVehicleProject())-m_local_reference_position);
        m_location = location;
        update();
    }
}

void ROSLink::updateOriginLocation(const QGeoCoordinate& location)
{
    setPos(geoToPixel(location,autonomousVehicleProject()));
    m_local_reference_position = geoToPixel(location,autonomousVehicleProject());
    m_have_local_reference = true;
}

void ROSLink::updateHeading(double heading)
{
    m_heading = heading;
    update();
}

void ROSLink::addAISContact(uint32_t mmsi, Contact c)
{
    if(m_have_local_reference)
    {
        c.location_local = geoToPixel(c.location,autonomousVehicleProject())-m_local_reference_position;
    }
    m_contacts[mmsi].push_back(c);
    update();
}

void ROSLink::updateBackground(BackgroundRaster *bgr)
{
    setParentItem(bgr);
    setPos(geoToPixel(m_origin,autonomousVehicleProject()));
    if(m_have_local_reference)
    {
        m_local_reference_position = geoToPixel(m_origin,autonomousVehicleProject());
        m_local_location_history.clear();
        for(auto l: m_location_history)
        {
            m_local_location_history.push_back(geoToPixel(l,autonomousVehicleProject())-m_local_reference_position);            
        }
        for(auto contactList: m_contacts)
        {
            for(auto contact: contactList.second)
                contact.location_local = geoToPixel(contact.location,autonomousVehicleProject())-m_local_reference_position;
        }
    }
}

bool ROSLink::active() const
{
    return m_active;
}

void ROSLink::setActive(bool active)
{
    m_active = active;
    std_msgs::Bool b;
    b.data = active;
    m_active_publisher.publish(b);
}

const std::string& ROSLink::helmMode() const
{
    return m_helmMode;
}

void ROSLink::setHelmMode(const std::string& helmMode)
{
    m_helmMode = helmMode;
    std_msgs::String hm;
    hm.data = helmMode;
    m_helmMode_publisher.publish(hm);
}

AutonomousVehicleProject * ROSLink::autonomousVehicleProject() const
{
    return qobject_cast<AutonomousVehicleProject*>(parent());
}
