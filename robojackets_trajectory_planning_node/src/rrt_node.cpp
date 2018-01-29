#include <string>
#include <ros/ros.h>
/*
#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QtQuick>
#include <QtWidgets>
#include <QQuickPaintedItem>
#include <iostream>
*/
#include <Eigen/Dense>
#include <memory> // for make_unique
#include "2dplane/GridStateSpace.hpp"
#include "BiRRT.hpp"
#include "2dplane/2dplane.hpp"
#include "planning/Path.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif


class PlannerWrapper
{
public:
    PlannerWrapper();
    void step();

    std::shared_ptr<RRT::GridStateSpace> _stateSpace;
    std::unique_ptr<RRT::BiRRT<Eigen::Vector2d>> _biRRT;

    Eigen::Vector2d _startVel;
    Eigen::Vector2d _goalVel;
    std::vector<Eigen::Vector2d> _previousSolution;
};

PlannerWrapper::PlannerWrapper()
{
    Eigen::Vector2d size(800, 600);
    _stateSpace = std::make_shared<RRT::GridStateSpace>(size.x(), size.y(), 40, 30);
    _biRRT = std::make_unique<RRT::BiRRT<Eigen::Vector2d>>(_stateSpace, 2);

    //  setup birrt
    _biRRT->setStartState(size / 10);
    _biRRT->setGoalState(size / 2);
    _biRRT->setMaxStepSize(30);
    _biRRT->setGoalMaxDist(12);

    _startVel = Eigen::Vector2d(3, 0);
    _goalVel = Eigen::Vector2d(0, 3);
}

void PlannerWrapper::step()
{
    int numTimes = 50;

    for (int i = 0; i < numTimes; i++)
    {
        _biRRT->grow();
    }

    ROS_INFO("#%d with size() = %d", _biRRT->iterationCount(), _biRRT->getPath().size());

    // store solution
    _previousSolution.clear();
    if (_biRRT->startSolutionNode() != nullptr)
    {
        ROS_INFO("   Found a solution");
        _previousSolution = _biRRT->getPath();
        RRT::SmoothPath(_previousSolution, *_stateSpace);
    }
}

////////////////////////////////////////////////////////////////////////////////////

class BasicDrawPane
    : public wxPanel
{
 
public:
    BasicDrawPane(wxFrame* parent);
 
    void paintEvent(wxPaintEvent & evt);
    void paintNow();
 
    void render(wxDC& dc);

    void SetPoints(std::vector<Eigen::Vector2d> points);
    void SetLines(std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d>> lines);
private: 
    std::vector<Eigen::Vector2d> _points;
    std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d>> _lines;

    wxDECLARE_EVENT_TABLE();
};

class wxApplicationNode
    : public wxApp
{
public:
    virtual bool OnInit();
    void DoUpdate(wxIdleEvent &event);
 
private:
    void ExtractPointsAndLinesFromTreeForDrawPane();

    wxFrame *frame;
    BasicDrawPane *drawPane;
    PlannerWrapper *_planner;
 
    wxDECLARE_EVENT_TABLE();
};

BasicDrawPane::BasicDrawPane(wxFrame* parent)
    : wxPanel(parent)
{
}
 
void BasicDrawPane::paintEvent(wxPaintEvent & evt)
{
    wxPaintDC dc(this);
    render(dc);
}
 
void BasicDrawPane::paintNow()
{
    wxClientDC dc(this);
    render(dc);
}

void BasicDrawPane::render(wxDC&  dc)
{ 
    //  node drawing radius
    const double r = 3;

    /// draw all points
    for(const Eigen::Vector2d& point : _points)
    {
        // circles
        dc.SetBrush(*wxGREEN_BRUSH); // green filling
        dc.SetPen( wxPen( wxColor(255,0,0), 2 ) ); // 2-pixels-thick red outline

        // draw a circle around p1 and p2
        dc.DrawCircle( wxPoint(point.x(),point.y()), r /* radius */ );
    }

    /// draw all lines
    for(const std::pair<Eigen::Vector2d, Eigen::Vector2d>& line : _lines)
    {
    }
    
    int p1_x = 200;
    int p1_y = 100;
    int p2_x = 220;
    int p2_y = 108;
    
    dc.DrawCircle( wxPoint(p2_x,p2_y), r /* radius */ );
 
    // draw a line from p1 to p2
    dc.SetPen( wxPen( wxColor(0,0,0), 1 ) ); // black line, 3 pixels thick
    dc.DrawLine( p1_x, p1_y, p2_x, p2_y ); // draw line across the rectangle
}

void BasicDrawPane::SetPoints(std::vector<Eigen::Vector2d> points)
{
    _points = points;
}

void BasicDrawPane::SetLines(std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d>> lines)
{
    _lines = lines;
}

////////////////////////////////////////////////////////////////////////////////////

wxBEGIN_EVENT_TABLE(wxApplicationNode, wxApp)
    EVT_IDLE(wxApplicationNode::DoUpdate)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(BasicDrawPane, wxPanel)
    EVT_PAINT(BasicDrawPane::paintEvent)
wxEND_EVENT_TABLE()

IMPLEMENT_APP_NO_MAIN(wxApplicationNode);
IMPLEMENT_WX_THEME_SUPPORT;

bool wxApplicationNode::OnInit()
{
    _planner = new PlannerWrapper();

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    frame = new wxFrame((wxFrame *)NULL, -1,  wxT("BiRRT"), wxPoint(50,50), wxSize(800,600));
    
    drawPane = new BasicDrawPane( (wxFrame*) frame );
    sizer->Add(drawPane, 1, wxEXPAND);
 
    frame->SetSizer(sizer);
    frame->SetAutoLayout(true);
 
    frame->Show(true);
    return true;
}

void wxApplicationNode::DoUpdate(wxIdleEvent &event)
{
    if(ros::ok())
    {
        _planner->step();
        ros::spinOnce();
	ros::Duration(1).sleep();
    }

    ExtractPointsAndLinesFromTreeForDrawPane();

    if(IsMainLoopRunning())
    {
        event.RequestMore();
    }
}

void wxApplicationNode::ExtractPointsAndLinesFromTreeForDrawPane()
{
    std::vector<Eigen::Vector2d> points;
    std::vector<std::pair<Eigen::Vector2d, Eigen::Vector2d>> lines;

    for(const RRT::Node<Eigen::Vector2d>& node : _planner->_biRRT->startTree().allNodes())
    {
	Eigen::Vector2d current(node.state().x(), node.state().y());
        points.push_back(current);
        if(node.parent())
        {
	    Eigen::Vector2d parent(node.parent()->state().x(), node.parent()->state().y());
            std::pair<Eigen::Vector2d, Eigen::Vector2d> line(current, parent);

            points.push_back(parent);
            lines.push_back(line);
        }
    }

    drawPane->SetPoints(points);
    drawPane->SetLines(lines);
}

////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{ 
    ros::init(argc, argv, "rrt_node");
    ros::NodeHandle node_handle;
    
    ROS_INFO("Starting RRT node with wxWidget visualization");

    wxEntryStart( argc, argv );
    wxTheApp->CallOnInit();
    wxTheApp->OnRun();

    /// running - loop see DoUpdate()

    wxTheApp->OnExit();
    wxEntryCleanup();

    return 0;
}