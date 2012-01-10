/* 
 * The Biomechanical ToolKit
 * Copyright (c) 2009-2012, Arnaud Barré
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name(s) of the copyright holders nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "MultiViewWidget.h"
#include "Acquisition.h"
#include "Model.h"
#include "CompositeView.h"
#include "ChartWidget.h"
#include "LoggerVTKOutput.h"
#include "Viz3DWidget.h"

#include <btkVTKInteractorStyleTrackballFixedUpCamera.h>
#include <btkVTKForcePlatformsSource.h>
#include <btkVTKGroundSource.h>
#include <btkVTKGRFsFramesSource.h>
#include <btkVTKCommandEvents.h>
#include <btkVTKMarkersFramesSource.h>
#include <btkVTKSegmentsFramesSource.h>

#include <QDragEnterEvent>
#include <QUrl>
#include <QSplitter>
#include <QCheckBox>

#include <vtkInformation.h>
#include <vtkIdList.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkCompositeDataPipeline.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkstd/list>
#include <vtkstd/map>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkCellPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkAxesActor.h>
#include <vtkPlaneSource.h>
#include <vtkObjectBase.h>
#include <vtkIdTypeArray.h>
#include <vtkCamera.h>

enum {VTK_GROUND, VTK_FORCE_PLATFORMS, VTK_MARKERS, VTK_GRFS, VTK_SEGMENTS};

class vtkStreamingDemandDrivenPipelineCollection : public vtkstd::list<vtkStreamingDemandDrivenPipeline*>
{};

class vtkProcessMap : public vtkstd::map<int, vtkObjectBase*>
{};

MultiViewWidget::MultiViewWidget(QWidget* parent)
: AbstractMultiView(parent), m_ForcePlatformColor(255, 255, 0), m_ForceVectorColor(255, 255, 0),
  m_View3dActions(), m_ViewChartActions()
{
  this->mp_EventFilterObject = 0;
  this->mp_Acquisition = 0;
  this->mp_Model = 0;
  this->mp_EventQtSlotConnections = vtkEventQtSlotConnect::New();
  this->mp_VTKProc = new vtkProcessMap();
  this->mp_Syncro = new vtkStreamingDemandDrivenPipelineCollection();
  this->mp_PointChartFrames = vtkDoubleArray::New();
  this->mp_PointChartFrames->SetName("Frame");
  this->mp_AnalogChartFrames = vtkDoubleArray::New();
  this->mp_AnalogChartFrames->SetName("Frame");
  this->mp_ForcePlatformActor = 0;
  this->mp_ForceVectorActor = 0;
  this->mp_Mappers = vtkMapperCollection::New();
  this->mp_GroupOrientationMenu = new QMenu(tr("Ground Orientation"),this);
  this->mp_ActionGroundOrientationAutomatic = new QAction(tr("Automatic"),this);
  this->mp_ActionGroundOrientationAutomatic->setCheckable(true);
  this->mp_ActionGroundOrientationPlaneXY = new QAction(tr("Plane XY"),this);
  this->mp_ActionGroundOrientationPlaneXY->setCheckable(true);
  this->mp_ActionGroundOrientationPlaneYZ = new QAction(tr("Plane YZ"),this);
  this->mp_ActionGroundOrientationPlaneYZ->setCheckable(true);
  this->mp_ActionGroundOrientationPlaneZX = new QAction(tr("Plane ZX"),this);
  this->mp_ActionGroundOrientationPlaneZX->setCheckable(true);
  QActionGroup* groundOrientationActionGroup = new QActionGroup(this);
  groundOrientationActionGroup->addAction(this->mp_ActionGroundOrientationAutomatic);
  groundOrientationActionGroup->addAction(this->mp_ActionGroundOrientationPlaneXY);
  groundOrientationActionGroup->addAction(this->mp_ActionGroundOrientationPlaneYZ);
  groundOrientationActionGroup->addAction(this->mp_ActionGroundOrientationPlaneZX);
  this->mp_ActionGroundOrientationAutomatic->setChecked(true);
  this->mp_GroupOrientationMenu->addAction(this->mp_ActionGroundOrientationAutomatic);
  this->mp_GroupOrientationMenu->addAction(this->mp_ActionGroundOrientationPlaneXY);
  this->mp_GroupOrientationMenu->addAction(this->mp_ActionGroundOrientationPlaneYZ);
  this->mp_GroupOrientationMenu->addAction(this->mp_ActionGroundOrientationPlaneZX);
  
  this->mp_MarkerTrajectoryLengthMenu = new QMenu(tr("Marker Trajectory Length"),this);
  this->mp_ActionMarkerTrajectoryFull = new QAction(tr("All Frames"),this);;
  this->mp_ActionMarkerTrajectoryFull->setCheckable(true);
  this->mp_ActionMarkerTrajectory25 = new QAction(tr("25 Frames"),this);
  this->mp_ActionMarkerTrajectory25->setCheckable(true);
  this->mp_ActionMarkerTrajectory50 = new QAction(tr("50 Frames"),this);
  this->mp_ActionMarkerTrajectory50->setCheckable(true);
  this->mp_ActionMarkerTrajectory100 = new QAction(tr("100 Frames"),this);
  this->mp_ActionMarkerTrajectory100->setCheckable(true);
  this->mp_ActionMarkerTrajectory200 = new QAction(tr("200 Frames"),this);
  this->mp_ActionMarkerTrajectory200->setCheckable(true);
  QActionGroup* trajectoryLengthActionGroup = new QActionGroup(this);
  trajectoryLengthActionGroup->addAction(this->mp_ActionMarkerTrajectoryFull);
  trajectoryLengthActionGroup->addAction(this->mp_ActionMarkerTrajectory25);
  trajectoryLengthActionGroup->addAction(this->mp_ActionMarkerTrajectory50);
  trajectoryLengthActionGroup->addAction(this->mp_ActionMarkerTrajectory100);
  trajectoryLengthActionGroup->addAction(this->mp_ActionMarkerTrajectory200);
  this->mp_ActionMarkerTrajectory100->setChecked(true);
  this->mp_MarkerTrajectoryLengthMenu->addAction(this->mp_ActionMarkerTrajectoryFull);
  this->mp_MarkerTrajectoryLengthMenu->addAction(this->mp_ActionMarkerTrajectory25);
  this->mp_MarkerTrajectoryLengthMenu->addAction(this->mp_ActionMarkerTrajectory50);
  this->mp_MarkerTrajectoryLengthMenu->addAction(this->mp_ActionMarkerTrajectory100);
  this->mp_MarkerTrajectoryLengthMenu->addAction(this->mp_ActionMarkerTrajectory200);
  
  connect(groundOrientationActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeGroundOrientation()));
  connect(trajectoryLengthActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(changeMarkerTrajectoryLength()));
  
  this->setViewPrototype(ViewFactory<CompositeView>);
};

MultiViewWidget::~MultiViewWidget()
{
  this->mp_EventQtSlotConnections->Delete();
  for (vtkProcessMap::iterator it = this->mp_VTKProc->begin() ; it != this->mp_VTKProc->end() ; ++it)
    it->second->Delete();
  delete this->mp_VTKProc;
  delete this->mp_Syncro;
  this->mp_PointChartFrames->Delete();
  this->mp_AnalogChartFrames->Delete();
  this->mp_Mappers->Delete();
  if (this->mp_ForcePlatformActor != 0) this->mp_ForcePlatformActor->Delete();
  if (this->mp_ForceVectorActor != 0) this->mp_ForceVectorActor->Delete();
  vtkAlgorithm::SetDefaultExecutivePrototype(0);
};

void MultiViewWidget::initialize()
{
  this->AbstractMultiView::initialize();
  
  CompositeView* sv = static_cast<CompositeView*>(static_cast<QGridLayout*>(this->layout())->itemAtPosition(0,0)->widget());
  vtkRenderer* renderer = static_cast<Viz3DWidget*>(sv->view(CompositeView::Viz3D))->renderer();
  ChartWidget* chart = static_cast<ChartWidget*>(sv->view(CompositeView::Chart));
  
  //vtkMapper::GlobalImmediateModeRenderingOn(); // For large dataset.
  
  // VTK PIPELINE
  // Static data
  // Simple ground grid
  btk::VTKGroundSource* ground = btk::VTKGroundSource::New();
  vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
  mapper->SetInput(ground->GetOutput());
  vtkProperty* prop = vtkProperty::New();
  prop->SetRepresentation(VTK_WIREFRAME);
  prop->SetColor(0.8, 0.8, 0.8);
  prop->SetAmbient(0.5);
  prop->SetDiffuse(0.0);
  prop->SetSpecular(0.0);
  vtkActor* actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetProperty(prop);
  actor->PickableOff();
  renderer->AddViewProp(actor);
  // Cleanup for ground
  mapper->Delete();
  actor->Delete();
  prop->Delete();
  // Camera
  renderer->GetActiveCamera()->Elevation(-60);
  renderer->ResetCamera();
  renderer->GetActiveCamera()->Zoom(1.6);
  
  // Dynamic data
  // Pipeline for force plaforms (plane)
  btk::VTKForcePlatformsSource* forcePlaforms = btk::VTKForcePlatformsSource::New();
  //forcePlaforms->SetInput(forcePlatformsExtractor->GetOutput());
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(forcePlaforms->GetOutputPort(0));
  prop = vtkProperty::New();
  prop->SetColor(this->m_ForcePlatformColor.redF(), this->m_ForcePlatformColor.greenF(), this->m_ForcePlatformColor.blueF());
  prop->SetOpacity(0.9);
  prop->SetAmbient(0.5);
  prop->SetDiffuse(0.0);
  prop->SetSpecular(0.0);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetScale(0.005);
  actor->SetProperty(prop);
  actor->PickableOff();
  renderer->AddActor(actor);
  this->mp_ForcePlatformActor = actor;
  // Cleanup for force platforms (plane)
  mapper->Delete();
  //actor->Delete();
  prop->Delete();
  // Pipeline for force plaforms (axes)
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(forcePlaforms->GetOutputPort(1));
  mapper->SetScalarModeToUseCellData();
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetScale(0.005);
  actor->SetProperty(prop);
  actor->PickableOff();
  renderer->AddActor(actor);
  // Cleanup for force platforms (axes)
  mapper->Delete();
  actor->Delete();
  // Pipeline for force plaforms (index)
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(forcePlaforms->GetOutputPort(2));
  prop = vtkProperty::New();
  prop->SetColor(1.0, 1.0, 1.0);
  prop->SetOpacity(0.9);
  prop->SetAmbient(0.5);
  prop->SetDiffuse(0.0);
  prop->SetSpecular(0.0);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetScale(0.005);
  actor->SetProperty(prop);
  actor->PickableOff();
  renderer->AddActor(actor);
  // Cleanup for force platforms (index)
  mapper->Delete();
  actor->Delete();
  prop->Delete();
  // Require to play with VTK information's keys TIME_*
  vtkCompositeDataPipeline* prototype = vtkCompositeDataPipeline::New();
  vtkAlgorithm::SetDefaultExecutivePrototype(prototype);
  prototype->Delete();
  // Pipeline for markers
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::New();
  //markers->SetInput(0, virtualMarkersSeparator->GetOutput(0));
  //markers->SetInput(1, virtualMarkersSeparator->GetOutput(2));
  // - Display marker's position
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(markers->GetOutputPort(0));
  mapper->SetLookupTable(markers->GetMarkerColorLUT());
  mapper->SetScalarModeToUsePointFieldData();
  mapper->UseLookupTableScalarRangeOn();
  mapper->SelectColorArray("Colors");
  this->mp_Mappers->AddItem(mapper);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetScale(0.005);
  renderer->AddActor(actor);
  mapper->Delete();
  actor->Delete();
  // - Display marker's trajectory
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(markers->GetOutputPort(1));
  mapper->SetLookupTable(markers->GetMarkerColorLUT());
  mapper->SetScalarModeToUsePointFieldData();
  mapper->UseLookupTableScalarRangeOn();
  mapper->SelectColorArray("Colors");
  this->mp_Mappers->AddItem(mapper);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->PickableOff();
  actor->SetScale(0.005);
  renderer->AddActor(actor);
  mapper->Delete();
  actor->Delete();
  // Pipeline for segments (links)
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::New();
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(segments->GetOutputPort(0));
  mapper->SetLookupTable(segments->GetSegmentColorLUT());
  mapper->SetScalarModeToUseCellData();
  mapper->UseLookupTableScalarRangeOn();
  mapper->SelectColorArray("Colors");
  this->mp_Mappers->AddItem(mapper);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetLineWidth(2);
  actor->SetScale(0.005);
  renderer->AddActor(actor);
  mapper->Delete();
  actor->Delete();
  // Pipeline for GRFs
  btk::VTKGRFsFramesSource* GRFs = btk::VTKGRFsFramesSource::New();
  //GRFs->SetInput(GRWsDownsampler->GetOutput());
  mapper = vtkPolyDataMapper::New();
  mapper->SetInputConnection(GRFs->GetOutputPort());
  this->mp_Mappers->AddItem(mapper);
  prop = vtkProperty::New();
  prop->SetColor(this->m_ForceVectorColor.redF(), this->m_ForceVectorColor.greenF(), this->m_ForceVectorColor.blueF());
  prop->SetAmbient(0.5);
  prop->SetDiffuse(0.0);
  prop->SetSpecular(0.0);
  actor = vtkActor::New();
  actor->SetMapper(mapper);
  actor->SetScale(0.005);
  actor->SetProperty(prop);
  actor->PickableOff();
  renderer->AddActor(actor);
  this->mp_ForceVectorActor = actor;
  // Cleanup for GRFs.
  mapper->Delete();
  //actor->Delete();
  prop->Delete();
  // Synchro between dynamic data
  this->mp_Syncro->push_back(vtkStreamingDemandDrivenPipeline::SafeDownCast(markers->GetExecutive()));
  this->mp_Syncro->push_back(vtkStreamingDemandDrivenPipeline::SafeDownCast(segments->GetExecutive()));
  this->mp_Syncro->push_back(vtkStreamingDemandDrivenPipeline::SafeDownCast(GRFs->GetExecutive()));
  // Store VTK process to be reused later.
  (*this->mp_VTKProc)[VTK_GROUND] = ground;
  (*this->mp_VTKProc)[VTK_FORCE_PLATFORMS] = forcePlaforms;
  (*this->mp_VTKProc)[VTK_MARKERS] = markers;
  (*this->mp_VTKProc)[VTK_GRFS] = GRFs;
  (*this->mp_VTKProc)[VTK_SEGMENTS] = segments;

  // Links between VTK & Qt
  this->mp_EventQtSlotConnections->Connect(
      markers, 
      btk::VTKMarkersListUpdatedEvent,
      this, 
      SLOT(updateDisplayedMarkersList(vtkObject*, unsigned long, void*, void*)));
      
  // Initialize the charts
  chart->setPointFrameArray(this->mp_PointChartFrames);
  chart->setAnalogFrameArray(this->mp_AnalogChartFrames);
  
  // Redirect the VTK message to the Logger
  vtkOutputWindow* w = LoggerVTKOutput::New();
  vtkOutputWindow::SetInstance(w);
  w->Delete();
};

void MultiViewWidget::setAcquisition(Acquisition* acq)
{
  if (this->mp_Acquisition)
    disconnect(this->mp_Acquisition, 0, this, 0);
  this->mp_Acquisition = acq;
  // Views
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<CompositeView*>(*it)->setAcquisition(this->mp_Acquisition);
  // BTK->VTK connection
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->SetInput(0, this->mp_Acquisition->btkMarkers());
  markers->SetInput(1, this->mp_Acquisition->btkVirtualMarkers());
  btk::VTKGRFsFramesSource* GRFs = btk::VTKGRFsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_GRFS]);
  GRFs->SetInput(this->mp_Acquisition->btkGroundReactionWrenches());
  btk::VTKForcePlatformsSource* forcePlaforms = btk::VTKForcePlatformsSource::SafeDownCast((*this->mp_VTKProc)[VTK_FORCE_PLATFORMS]);
  forcePlaforms->SetInput(this->mp_Acquisition->btkForcePlatforms());
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  segments->SetInput(this->mp_Acquisition->btkMarkers());
  // Object connection
  connect(this->mp_Acquisition, SIGNAL(markersRadiusChanged(QVector<int>, QVector<double>)), this, SLOT(setMarkersRadius(QVector<int>, QVector<double>)));
  connect(this->mp_Acquisition, SIGNAL(markersColorChanged(QVector<int>, QVector<QColor>)), this, SLOT(setMarkersColor(QVector<int>, QVector<QColor>)));
  connect(this->mp_Acquisition, SIGNAL(firstFrameChanged(int)), this, SLOT(updateChartFramesIndex(int)));
}

void MultiViewWidget::setModel(Model* m)
{
  if (this->mp_Model)
    disconnect(this->mp_Model, 0, this, 0);
  this->mp_Model = m;
  // Object connection
  connect(this->mp_Model, SIGNAL(segmentsColorChanged(QVector<int>, QVector<QColor>)), this, SLOT(setSegmentsColor(QVector<int>, QVector<QColor>)));
  connect(this->mp_Model, SIGNAL(segmentLinksChanged(int, QVector<int>, QVector< QPair<int,int> >)), this, SLOT(setSegmentLink(int, QVector<int>, QVector< QPair<int,int> >)));
}

void MultiViewWidget::setView3dActions(QList<QAction*> actions)
{
  this->m_View3dActions = actions;
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
  {
    CompositeView* sv = static_cast<CompositeView*>(*it);
    QWidget* w = sv->view(CompositeView::Viz3D);
    w->insertActions(0, this->m_View3dActions);
    w->setContextMenuPolicy(Qt::ActionsContextMenu);
  }
};

void MultiViewWidget::setViewChartActions(QList<QAction*> actions)
{
  this->m_ViewChartActions = actions;
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<ChartWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Chart))->addActions(this->m_ViewChartActions);
};

void MultiViewWidget::load()
{
  if (!this->mp_Acquisition)
    return;
  
  QString markerUnit = this->mp_Acquisition->pointUnit(Point::Marker);
  double scale = 1.0;
  if (markerUnit.compare("m") == 0)
    scale = 1000.0;
  
  // Update the 3D view
  btk::VTKGroundSource* ground = btk::VTKGroundSource::SafeDownCast((*this->mp_VTKProc)[VTK_GROUND]);
  ground->SetInput(this->mp_Acquisition->btkAcquisition());
  ground->Update();
  btk::VTKGRFsFramesSource* GRFs = btk::VTKGRFsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_GRFS]);
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  btk::VTKForcePlatformsSource* forcePlaforms = btk::VTKForcePlatformsSource::SafeDownCast((*this->mp_VTKProc)[VTK_FORCE_PLATFORMS]);
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  GRFs->SetScaleUnit(scale);
  markers->SetScaleUnit(scale);
  forcePlaforms->SetScaleUnit(scale);
  segments->ClearDefinitions(); // Reset the segments.
  segments->SetScaleUnit(scale);

  this->updateCameras();
  this->updateDisplay(this->mp_Acquisition->firstFrame());
  
  // Update the X axis values for the charts
  this->mp_PointChartFrames->Initialize(); // Reset
  this->mp_PointChartFrames->SetNumberOfValues(this->mp_Acquisition->pointFrameNumber());
  this->mp_AnalogChartFrames->Initialize(); // Reset
  this->mp_AnalogChartFrames->SetNumberOfValues(this->mp_Acquisition->analogFrameNumber());
  double sub = 1.0 / (double)this->mp_Acquisition->analogSamplePerPointFrame();
  for (int i = 0 ; i < this->mp_Acquisition->pointFrameNumber() ; ++i)
  {
    // Point
    this->mp_PointChartFrames->SetValue(i, this->mp_Acquisition->firstFrame() + i);
    // Analog
    int inc = i * this->mp_Acquisition->analogSamplePerPointFrame();
    double val = static_cast<double>(this->mp_Acquisition->firstFrame() + i);
    this->mp_AnalogChartFrames->SetValue(inc, val);
    for (int j = 1 ; j < this->mp_Acquisition->analogSamplePerPointFrame() ; ++j)
      this->mp_AnalogChartFrames->SetValue(inc + j, val + j * sub);
  }
  // Force the update for the generation of the force platforms and their forces (In case there is no 3D view, they are not updated).
  forcePlaforms->Update();
  GRFs->Update();
  // Active the content of each view
  for (QList<AbstractView*>::const_iterator it = this->views().begin() ; it != this->views().end() ; ++it)
    static_cast<CompositeView*>(*it)->show(true);
};

void MultiViewWidget::setCurrentFrameFunctor(btk::VTKCurrentFrameFunctor::Pointer functor)
{
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<ChartWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Chart))->setCurrentFrameFunctor(functor);
};

void MultiViewWidget::setRegionOfInterestFunctor(btk::VTKRegionOfInterestFunctor::Pointer functor)
{
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<ChartWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Chart))->setRegionOfInterestFunctor(functor);
};

void MultiViewWidget::setEventsFunctor(btk::VTKEventsFunctor::Pointer functor)
{
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<ChartWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Chart))->setEventsFunctor(functor);
};

void MultiViewWidget::setEventFilterObject(QObject* filter)
{
  this->mp_EventFilterObject = filter;
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    for (int i = 0 ; i < (*it)->viewStack->count() ; ++i)
      (*it)->viewStack->widget(i)->installEventFilter(this->mp_EventFilterObject);
};

static const int MultiViewWidgetMagic = 0x0abc;
static const int MultiViewWidgetVersion = 0x0100; // 1.0 
static const int MultiViewWidgetViewId = 0x0001;
static const int MultiViewWidgetSplitterId = 0x0002;

QByteArray MultiViewWidget::saveLayout() const
{
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_4_6);
  
  stream << qint32(MultiViewWidgetMagic);
  stream << qint32(MultiViewWidgetVersion);
  
  if (!this->saveLayout(stream, static_cast<QGridLayout*>(this->layout())->itemAtPosition(0,0)->widget()))
    return QByteArray();
  
  return data;
};

bool MultiViewWidget::saveLayout(QDataStream& stream, QWidget* w) const
{
  QSplitter* splitter = qobject_cast<QSplitter*>(w);
  if (splitter)
  {
    float ratio = 0.0f;
    QList<int> sizes = splitter->sizes();
    if (splitter->orientation() == Qt::Vertical)
      ratio = (float)sizes[0] / (float)splitter->height();
    else
      ratio = (float)sizes[0] / (float)splitter->width();
    
    stream << qint32(MultiViewWidgetSplitterId);
    stream << qint32(splitter->orientation());
    stream << ratio;
    
    if (!this->saveLayout(stream, splitter->widget(0)) || !this->saveLayout(stream, splitter->widget(1)))
      return false;
  }
  else
  {
    CompositeView* view = qobject_cast<CompositeView*>(w);
    if (view == 0)
    {
      qCritical("Impossible to save the layout of the view. One of the given widget, is not a view.");
      return false;
    }
    stream << qint32(MultiViewWidgetViewId);
    stream << qint32(view->viewCombo->currentIndex());
    // WARNING: The following code is very dependant of the order of the widget set in the composite view.
    //          Its modification must be reflected here. Otherwise, the layout will be corrupted.
    // View options
    switch(view->viewCombo->currentIndex())
    {
    case 2: // Orthogonal 3D view
      stream << qint32(static_cast<QComboBox*>(view->optionStack->currentWidget())->currentIndex());
      break;
    case 4: // Point chart
      stream << qint32(static_cast<QCheckBox*>(view->optionStack->currentWidget()->layout()->itemAt(0)->layout()->itemAt(0)->widget())->checkState());
      stream << qint32(static_cast<QCheckBox*>(view->optionStack->currentWidget()->layout()->itemAt(0)->layout()->itemAt(2)->widget())->checkState());
      stream << qint32(static_cast<QCheckBox*>(view->optionStack->currentWidget()->layout()->itemAt(0)->layout()->itemAt(4)->widget())->checkState());
      break;
    case 5: // Analog chart
      stream << qint32(static_cast<QComboBox*>(view->optionStack->currentWidget()->layout()->itemAt(0)->layout()->itemAt(0)->widget())->currentIndex());
      break;
    }
  }
  return true;
};

bool MultiViewWidget::restoreLayout(const QByteArray& state)
{
  QByteArray sd = state;
  QDataStream stream(&sd, QIODevice::ReadOnly);
  
  qint32 magic;
  qint32 version;
  
  stream >> magic;
  stream >> version;
  if (magic != MultiViewWidgetMagic)
  {
    qCritical("The data given to set the layout of the views are not recognized.");
    return false;
  }
  if (version == 0x0100)
  {
    stream.setVersion(QDataStream::Qt_4_6);
    this->closeAll();
    CompositeView* view = static_cast<CompositeView*>(static_cast<QGridLayout*>(this->layout())->itemAtPosition(0,0)->widget());
    return this->restoreLayout(stream, view, this->size());
  }
  else
    qCritical("Unknwon version number for serialized data containing the layout of the views. You may have used a new version containing some major changes in the data format.");
  return false;
};

bool MultiViewWidget::restoreLayout(QDataStream& stream, CompositeView* view, const QSize& size)
{
  int id;
  stream >> id;
  if (id == MultiViewWidgetSplitterId)
  {
    AbstractView* views[2];
    QSize viewsSize[2];
    int orientation;
    float ratio;
    int wol; //width or length
    stream >> orientation;
    stream >> ratio;
    
    QSplitter* splitter = this->split(view, orientation, views);
    if (orientation == Qt::Horizontal)
    {
      wol = size.width() - splitter->handleWidth();
      viewsSize[0].setWidth(ratio * wol);
      viewsSize[1].setWidth((1.0 - ratio) * wol);
      viewsSize[0].setHeight(size.height());
      viewsSize[1].setHeight(size.height());
    }
    else if (orientation == Qt::Vertical)
    {
      wol = size.height() - splitter->handleWidth();
      viewsSize[0].setWidth(size.width());
      viewsSize[1].setWidth(size.width());
      viewsSize[0].setHeight(ratio * wol);
      viewsSize[1].setHeight((1.0 - ratio) * wol);
    }
    else
    {
      qCritical("Unknown orientation when extracting data to set the layout of the views.");
      return false;
    }
    
    if (!this->restoreLayout(stream, static_cast<CompositeView*>(views[0]), viewsSize[0]) || !this->restoreLayout(stream, static_cast<CompositeView*>(views[1]), viewsSize[1]))
      return false;
    
    QList<int> sizes; sizes << ratio * wol << (1.0 - ratio) * wol;
    splitter->setSizes(sizes);
  }
  else if (id == MultiViewWidgetViewId)
  {
    int index;
    stream >> index;
    view->viewCombo->setCurrentIndex(index);
    int optionIndex = view->optionStackIndexFromViewComboIndex(index);
    switch(index)
    {
    case 2: // Orthogonal 3D view
      {
      int index2;
      stream >> index2;
      static_cast<QComboBox*>(view->optionStack->widget(optionIndex))->setCurrentIndex(index2);
      break;
      }
    case 4: // Point chart
      {
      int checked;
      stream >> checked;
      static_cast<QCheckBox*>(view->optionStack->widget(optionIndex)->layout()->itemAt(0)->layout()->itemAt(0)->widget())->setCheckState(checked == 0 ? Qt::Unchecked : Qt::Checked);
      stream >> checked;
      static_cast<QCheckBox*>(view->optionStack->widget(optionIndex)->layout()->itemAt(0)->layout()->itemAt(2)->widget())->setCheckState(checked == 0 ? Qt::Unchecked : Qt::Checked);
      stream >> checked;
      static_cast<QCheckBox*>(view->optionStack->widget(optionIndex)->layout()->itemAt(0)->layout()->itemAt(4)->widget())->setCheckState(checked == 0 ? Qt::Unchecked : Qt::Checked);
      break;
      }
    case 5: // Analog chart
      {
      int index2;
      stream >> index2;
      static_cast<QComboBox*>(view->optionStack->widget(optionIndex)->layout()->itemAt(0)->layout()->itemAt(0)->widget())->setCurrentIndex(index2);
      break;
      }
    }
  }
  else
  {
    qCritical("Unknown ID when extracting data to set the layout of the views.");
    return false;
  }
  return true;
};

void MultiViewWidget::restoreLayout3DOnly()
{
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_4_6);
  
  stream << qint32(MultiViewWidgetMagic);
  stream << qint32(MultiViewWidgetVersion);
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(1); // 1: Perspective 3D
  
  this->restoreLayout(data);
};

void MultiViewWidget::restoreLayout3DVerbose()
{
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_4_6);
  
  stream << qint32(MultiViewWidgetMagic);
  stream << qint32(MultiViewWidgetVersion);
  stream << qint32(MultiViewWidgetSplitterId);
  stream << qint32(Qt::Vertical);
  stream << 0.8f;
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(1); // 1: Perspective 3D
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(7); // 7: Logger
  
  this->restoreLayout(data);
};

void MultiViewWidget::restoreLayout3DCharts()
{
  QByteArray data;
  QDataStream stream(&data, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_4_6);
  
  stream << qint32(MultiViewWidgetMagic);
  stream << qint32(MultiViewWidgetVersion);
  stream << qint32(MultiViewWidgetSplitterId);
  stream << qint32(Qt::Horizontal);
  stream << 0.5f;
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(1); // 1: Perspective 3D
  stream << qint32(MultiViewWidgetSplitterId);
  stream << qint32(Qt::Vertical);
  stream << 0.5f;
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(4); // 4: Point chart
  stream << qint32(Qt::Checked); // Component X displayed
  stream << qint32(Qt::Checked); // Component Y displayed
  stream << qint32(Qt::Checked); // Component Z displayed
  stream << qint32(MultiViewWidgetViewId);
  stream << qint32(5); // 5: Analog chart
  stream << qint32(0); // Collapsed mode
  
  this->restoreLayout(data);
};

void MultiViewWidget::updateChartFramesIndex(int ff)
{
  Q_UNUSED(ff)
  double sub = 1.0 / (double)this->mp_Acquisition->analogSamplePerPointFrame();
  for (int i = 0 ; i < this->mp_Acquisition->pointFrameNumber() ; ++i)
  {
    // Point
    this->mp_PointChartFrames->SetValue(i, this->mp_Acquisition->firstFrame() + i);
    // Analog
    int inc = i * this->mp_Acquisition->analogSamplePerPointFrame();
    double val = static_cast<double>(this->mp_Acquisition->firstFrame() + i);
    this->mp_AnalogChartFrames->SetValue(inc, val);
    for (int j = 1 ; j < this->mp_Acquisition->analogSamplePerPointFrame() ; ++j)
      this->mp_AnalogChartFrames->SetValue(inc + j, val + j * sub);
  }
  for (QList<AbstractView*>::const_iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
  {
    static_cast<ChartWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Chart))->updateAxisX();
    static_cast<CompositeView*>(*it)->render();
  }
};

void MultiViewWidget::appendNewSegments(const QList<int>& ids, const QList<Segment*>& segments)
{
  Q_UNUSED(ids);
  btk::VTKSegmentsFramesSource* segmentsFramesSource = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  QVector<QColor> colors(segments.size());
  // Assume new segments are appended at the end. No use of the IDs
  int inc = 0;
  for (QList<Segment*>::const_iterator it = segments.begin() ; it != segments.end() ; ++it)
  {
    colors[inc++] = (*it)->color;
    std::vector<int> btkPointIds((*it)->markerIds.size());
    std::vector<btk::VTKSegmentsFramesSource::Link> btkLinks((*it)->links.size());
    for (int i = 0 ; i < (*it)->markerIds.size() ; ++i)
      btkPointIds[i] = (*it)->markerIds[i];
    for (int i = 0 ; i < (*it)->links.size() ; ++i)
    {
      btkLinks[i].first = (*it)->links[i].first;
      btkLinks[i].second = (*it)->links[i].second;
    }
    segmentsFramesSource->AppendDefinition(btkPointIds, btkLinks);
  }
  this->setSegmentsColor(ids.toVector(), colors);
};

/*
void MultiViewWidget::appendNewSegment(const QVector<int>& markerIds, const QVector< QPair<int,int> >& links)
{
  std::vector<int> btkPointIds(markerIds.size());
  std::vector<btk::VTKSegmentsFramesSource::Link> btkLinks(links.size());
  for (int i = 0 ; i < markerIds.size() ; ++i)
    btkPointIds[i] = markerIds[i];
  for (int i = 0 ; i < links.size() ; ++i)
  {
    btkLinks[i].first = links[i].first;
    btkLinks[i].second = links[i].second;
  }
  btk::VTKSegmentsFramesSource* segmentsFramesSource = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  segmentsFramesSource->AppendDefinition(btkPointIds, btkLinks);
};
*/

void MultiViewWidget::clearSegments()
{
  btk::VTKSegmentsFramesSource* segmentsFramesSource = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  segmentsFramesSource->ClearDefinitions();
};

void MultiViewWidget::setSegmentsColor(const QVector<int>& ids, const QVector<QColor>& colors)
{
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  vtkLookupTable* segmentsColorsLUT = segments->GetSegmentColorLUT();
  for (int i = 0 ; i < ids.count() ; ++i)
  {
    int num = segmentsColorsLUT->GetNumberOfTableValues();
    QColor color = colors[i];
    int j = 0;
    while (j < num)
    {
      double* c = segmentsColorsLUT->GetTableValue(j);
      if ((color.redF() == c[0]) && (color.greenF() == c[1]) && (color.blueF() == c[2]))
      {
        segments->SetSegmentColorIndex(ids[i], j);
        break;
      }
      ++j;
    }
    if (j >= num)
    {
      segmentsColorsLUT->SetNumberOfTableValues(num + 1);
      segmentsColorsLUT->SetTableValue(num, color.redF(), color.greenF(), color.blueF());
      segmentsColorsLUT->SetTableRange(0, num + 1);
      segments->SetSegmentColorIndex(ids[i], num);
    }
  }
  this->updateSegmentsDisplay();
};

void MultiViewWidget::setSegmentLink(int id, const QVector<int>& markerIds, const QVector< QPair<int,int> >& links)
{
  std::vector<int> btkPointIds(markerIds.size());
  std::vector<btk::VTKSegmentsFramesSource::Link> btkLinks(links.size());
  for (int i = 0 ; i < markerIds.size() ; ++i)
    btkPointIds[i] = markerIds[i];
  for (int i = 0 ; i < links.size() ; ++i)
  {
    btkLinks[i].first = links[i].first;
    btkLinks[i].second = links[i].second;
  }
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  segments->SetDefinition(id, btkPointIds, btkLinks);
  this->updateSegmentsDisplay();
};

void MultiViewWidget::updateHiddenSegments(const QList<int>& ids)
{
  btk::VTKSegmentsFramesSource* segmentsFramesSource = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  segmentsFramesSource->ShowSegments();
  for (int i = 0 ; i < ids.count() ; ++i)
    segmentsFramesSource->HideSegment(ids[i]);
  this->updateSegmentsDisplay();
};

int MultiViewWidget::segmentColorIndex(int id)
{
  btk::VTKSegmentsFramesSource* segments = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS]);
  return segments->GetSegmentColorIndex(id);
};

void MultiViewWidget::setMarkerRadius(int id, double r)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->SetMarkerRadius(id, r);
};

void MultiViewWidget::setMarkersRadius(const QVector<int>& ids, const QVector<double>& radii)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  for (int i = 0 ; i < ids.count() ; ++i)
    markers->SetMarkerRadius(ids[i], radii[i]);
  this->updateMarkersDisplay();
};

void MultiViewWidget::setMarkersColor(const QVector<int>& ids, const QVector<QColor>& colors)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  vtkLookupTable* markersColorsLUT = markers->GetMarkerColorLUT();
  for (int i = 0 ; i < ids.count() ; ++i)
  {
    int num = markersColorsLUT->GetNumberOfTableValues();
    QColor color = colors[i];
    int j = 0;
    while (j < num)
    {
      double* c = markersColorsLUT->GetTableValue(j);
      if ((color.redF() == c[0]) && (color.greenF() == c[1]) && (color.blueF() == c[2]))
      {
        markers->SetMarkerColorIndex(ids[i], j);
        break;
      }
      ++j;
    }
    if (j >= num)
    {
      markersColorsLUT->SetNumberOfTableValues(num + 1);
      markersColorsLUT->SetTableValue(num, color.redF(), color.greenF(), color.blueF());
      markersColorsLUT->SetTableRange(0, num + 1);
      markers->SetMarkerColorIndex(ids[i], num);
    }
  }
  this->updateMarkersDisplay();
};

void MultiViewWidget::updateHiddenMarkers(const QList<int>& ids)
{
  btk::VTKMarkersFramesSource* markersFramesSource = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markersFramesSource->ShowMarkers();
  for (int i = 0 ; i < ids.count() ; ++i)
    markersFramesSource->HideMarker(ids[i]);
  this->updateMarkersDisplay();
};

void MultiViewWidget::updateTrackedMarkers(const QList<int>& ids)
{
  btk::VTKMarkersFramesSource* markersFramesSource = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markersFramesSource->HideTrajectories();
  for (int i = 0 ; i < ids.count() ; ++i)
    markersFramesSource->ShowTrajectory(ids[i]);
  this->updateMarkersDisplay();
};

double MultiViewWidget::markerRadius(int id)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  return markers->GetMarkerRadius(id);
};

void MultiViewWidget::setMarkerColorIndex(int id, int idx)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->SetMarkerColorIndex(id, idx);
};

int MultiViewWidget::markerColorIndex(int id)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  return markers->GetMarkerColorIndex(id);
};

void MultiViewWidget::setMarkerVisibility(int id, bool visible)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->SetMarkerVisibility(id, visible);
};

bool MultiViewWidget::markerVisibility(int id)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  return markers->GetMarkerVisibility(id);
};

double* MultiViewWidget::markerColorValue(int c)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  return markers->GetMarkerColorLUT()->GetTableValue(c);
}

bool MultiViewWidget::appendNewMarkerColor(const QColor& color, int* idx)
{
  bool modified = false;
  vtkLookupTable* markersColorsLUT = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->GetMarkerColorLUT();
  int num = markersColorsLUT->GetNumberOfTableValues();
  int i = 0;
  for (i = 0 ; i < num ; ++i)
  {
    double* c = markersColorsLUT->GetTableValue(i);
    if ((color.redF() == c[0]) && (color.greenF() == c[1]) && (color.blueF() == c[2]))
      break;
  }
  if (i >= num)
  {
    markersColorsLUT->SetNumberOfTableValues(num + 1);
    markersColorsLUT->SetTableValue(num, color.redF(), color.greenF(), color.blueF());
    markersColorsLUT->SetTableRange(0, num + 1);
    i = num;
    modified = true;
  }
  *idx = i;
  return modified;
};

const QString MultiViewWidget::groundNormalAsString() const
{
  QString str = "+Z";
  btk::VTKGroundSource* ground = btk::VTKGroundSource::SafeDownCast((*this->mp_VTKProc)[VTK_GROUND]);
  double n[3]; ground->GetNormal(n);
  if (n[0] == 1.0)
    str = "+X";
  else if (n[1] == 1.0)
    str = "+Y";
  return str;
};

void MultiViewWidget::setDefaultGroundOrientation(int index)
{
  btk::VTKGroundSource* ground = btk::VTKGroundSource::SafeDownCast((*this->mp_VTKProc)[VTK_GROUND]);
  if (index == 0)
    ground->SetAutomaticDefaultOrientation(btk::VTKGroundSource::PlaneXY);
  else if (index == 1)
    ground->SetAutomaticDefaultOrientation(btk::VTKGroundSource::PlaneYZ);
  else if (index == 2)
    ground->SetAutomaticDefaultOrientation(btk::VTKGroundSource::PlaneZX);
  else
  {
    qDebug("Unknown index to set the default ground orientation");
    return;
  }
    
  if (ground->GetOrientation() == btk::VTKGroundSource::Automatic)
  {
    this->updateCameras();
    this->updateViews();
  }
};

void MultiViewWidget::setDefaultSegmentColor(const QColor& color)
{
  this->mp_Model->setDefaultSegmentColor(color);
  vtkLookupTable* segmentsColorsLUT = btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS])->GetSegmentColorLUT();
  segmentsColorsLUT->SetTableValue(0, color.redF(), color.greenF(), color.blueF());
  this->updateViews();
};

void MultiViewWidget::setDefaultMarkerRadius(double r)
{
  this->mp_Acquisition->setDefaultMarkerRadius(r);
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->SetDefaultMarkerRadius(r);
};

void MultiViewWidget::setDefaultMarkerColor(const QColor& color)
{
  this->mp_Acquisition->setDefaultMarkerColor(color);
  vtkLookupTable* markersColorsLUT = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->GetMarkerColorLUT();
  markersColorsLUT->SetTableValue(0, color.redF(), color.greenF(), color.blueF());
  this->updateViews();
};

void MultiViewWidget::setMarkerTrajectoryLength(int index)
{
  if (index == 0) // All
    this->mp_ActionMarkerTrajectoryFull->trigger();
  else if (index == 1) // 25 frames
    this->mp_ActionMarkerTrajectory25->trigger();
  else if (index == 2) // 50 frames
    this->mp_ActionMarkerTrajectory50->trigger();
  else if (index == 3) // 100 frames
    this->mp_ActionMarkerTrajectory100->trigger();
  else if (index == 4) // 200 frames
    this->mp_ActionMarkerTrajectory200->trigger();
};

void MultiViewWidget::showForcePlatformAxes(bool isShown)
{
  btk::VTKForcePlatformsSource* forcePlaforms = btk::VTKForcePlatformsSource::SafeDownCast((*this->mp_VTKProc)[VTK_FORCE_PLATFORMS]);
  forcePlaforms->SetShowAxes(isShown);
  this->updateMarkersDisplay();
};

void MultiViewWidget::showForcePlatformIndex(bool isShown)
{
  btk::VTKForcePlatformsSource* forcePlaforms = btk::VTKForcePlatformsSource::SafeDownCast((*this->mp_VTKProc)[VTK_FORCE_PLATFORMS]);
  forcePlaforms->SetShowIndex(isShown);
  this->updateMarkersDisplay();
};

void MultiViewWidget::setForcePlatformColor(const QColor& color)
{
  this->m_ForcePlatformColor = color;
  if (!this->mp_ForcePlatformActor)
    return;
  this->mp_ForcePlatformActor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
  this->updateViews();
};

void MultiViewWidget::setForceVectorColor(const QColor& color)
{
  this->m_ForceVectorColor = color;
  if (!this->mp_ForceVectorActor)
    return;
  this->mp_ForceVectorActor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
  this->updateViews();
};

void MultiViewWidget::updateDisplayedMarkersList(vtkObject* /* caller */, unsigned long /* vtk_event */, void* /* client_data */, void* call_data)
{
  vtkIntArray* indexes = static_cast<vtkIntArray*>(call_data);
  if (!indexes)
    return;
    
  QVector<int> ids = QVector<int>(indexes->GetSize());
  for (int i = 0; i < ids.size() ; ++i)
    ids[i] = (indexes->GetValue(i) == -1 ? 0 : 1);
  emit visibleMarkersChanged(ids);
};

void MultiViewWidget::clear()
{
  for (QList<AbstractView*>::const_iterator it = this->views().begin() ; it != this->views().end() ; ++it)
    static_cast<CompositeView*>(*it)->show(false);
};

void MultiViewWidget::circleSelectedMarkers(const QList<int>& ids)
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  markers->ClearSelectedMarkers();
  for (QList<int>::const_iterator it = ids.begin() ; it != ids.end() ; ++it)
    markers->SetSelectedMarker(*it);
  this->updateMarkersDisplay();
};

void MultiViewWidget::updateSegmentsDisplay()
{
  btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  this->mp_Mappers->InitTraversal();
  vtkMapper* mapper;
  while ((mapper = this->mp_Mappers->GetNextItem()) != NULL)
    mapper->Modified();
  this->updateViews();
};

void MultiViewWidget::updateMarkersDisplay()
{
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  this->mp_Mappers->InitTraversal();
  vtkMapper* mapper;
  while ((mapper = this->mp_Mappers->GetNextItem()) != NULL)
    mapper->Modified();
  this->updateViews();
};

void MultiViewWidget::updateDisplay()
{
  // Force to update the markers even if the required frame was the last updated.
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  // Same thing for the segments
  btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  // Update
  this->mp_Mappers->InitTraversal();
  vtkMapper* mapper;
  while ((mapper = this->mp_Mappers->GetNextItem()) != NULL)
    mapper->Modified();
  this->updateViews();
};

void MultiViewWidget::updateDisplay(int frame)
{
  // Force to update the markers even if the required frame was the last updated.
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  // Same thing for the segments
  btk::VTKSegmentsFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_SEGMENTS])->GetOutput()->GetInformation()->Remove(vtkDataObject::DATA_TIME_STEPS());
  // Update
  double t = static_cast<double>(frame - this->mp_Acquisition->firstFrame());
  for (vtkStreamingDemandDrivenPipelineCollection::iterator it = this->mp_Syncro->begin() ; it != this->mp_Syncro->end() ; ++it)
    (*it)->SetUpdateTimeStep(0, t);
  this->mp_Mappers->InitTraversal();
  vtkMapper* mapper;
  while ((mapper = this->mp_Mappers->GetNextItem()) != NULL)
    mapper->Modified();
  this->updateViews();
};

void MultiViewWidget::showAllMarkers()
{
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->ShowMarkers();
  this->updateMarkersDisplay();
};

void MultiViewWidget::hideAllMarkers()
{
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->HideMarkers();
  this->updateMarkersDisplay();
};

void MultiViewWidget::forceRubberBandDrawingOn()
{
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<btk::VTKInteractorStyleTrackballFixedUpCamera*>(static_cast<Viz3DWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Viz3D))->GetRenderWindow()->GetInteractor()->GetInteractorStyle())->ForceRubberBandDrawingOn();
};

void MultiViewWidget::forceRubberBandDrawingOff()
{
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<btk::VTKInteractorStyleTrackballFixedUpCamera*>(static_cast<Viz3DWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Viz3D))->GetRenderWindow()->GetInteractor()->GetInteractorStyle())->ForceRubberBandDrawingOff();
};

void MultiViewWidget::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat("text/uri-list"))
    event->acceptProposedAction();
};

void MultiViewWidget::dropEvent(QDropEvent *event)
{
  QList<QUrl> urls = event->mimeData()->urls();
  if (urls.isEmpty())
      return;

  QString filename = urls.first().toLocalFile();
  if (filename.isEmpty())
      return;
  
  event->acceptProposedAction();
  emit fileDropped(filename);
};

AbstractView* MultiViewWidget::createView(AbstractView* fromAnother)
{
  CompositeView* sv = static_cast<CompositeView*>(this->AbstractMultiView::createView(fromAnother));
  sv->copyOptions(static_cast<CompositeView*>(fromAnother));
  // Viz3D final settings
  Viz3DWidget* viz3D = static_cast<Viz3DWidget*>(sv->view(CompositeView::Viz3D));
  connect(viz3D, SIGNAL(pickedMarkerChanged(int)), this, SIGNAL(pickedMarkerChanged(int)));
  connect(viz3D, SIGNAL(pickedMarkerToggled(int)), this, SIGNAL(pickedMarkerToggled(int)));
  connect(viz3D, SIGNAL(selectedMarkersToggled(QList<int>)), this, SIGNAL(selectedMarkersToggled(QList<int>)));
  connect(viz3D, SIGNAL(trajectoryMarkerToggled(int)), this, SIGNAL(trajectoryMarkerToggled(int)));
  viz3D->addActions(this->m_View3dActions);
  viz3D->setContextMenuPolicy(Qt::ActionsContextMenu);
  // Chart final settings
  ChartWidget* chart2D = static_cast<ChartWidget*>(sv->view(CompositeView::Chart));
  chart2D->addActions(this->m_ViewChartActions);
  connect(chart2D, SIGNAL(pausePlaybackRequested(bool)), this, SIGNAL(pausePlaybackRequested(bool)));
  // Event filter
  if (this->mp_EventFilterObject)
  {
    for (int i = 0 ; i < sv->viewStack->count() ; ++i)
      sv->viewStack->widget(i)->installEventFilter(this->mp_EventFilterObject);
  }
  return sv;
};

void MultiViewWidget::changeGroundOrientation()
{
  btk::VTKGroundSource* ground = btk::VTKGroundSource::SafeDownCast((*this->mp_VTKProc)[VTK_GROUND]);
  if (this->mp_ActionGroundOrientationAutomatic->isChecked())
    ground->SetOrientation(btk::VTKGroundSource::Automatic);
  else if (this->mp_ActionGroundOrientationPlaneXY->isChecked())
    ground->SetOrientation(btk::VTKGroundSource::PlaneXY);
  else if (this->mp_ActionGroundOrientationPlaneYZ->isChecked())
    ground->SetOrientation(btk::VTKGroundSource::PlaneYZ);
  else if (this->mp_ActionGroundOrientationPlaneZX->isChecked())
    ground->SetOrientation(btk::VTKGroundSource::PlaneZX);
    
  this->updateCameras();
  this->updateViews();
};

void MultiViewWidget::changeMarkerTrajectoryLength()
{
  btk::VTKMarkersFramesSource* markers = btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS]);
  if (this->mp_ActionMarkerTrajectoryFull->isChecked())
    markers->SetTrajectoryLength(-1);
  else if (this->mp_ActionMarkerTrajectory25->isChecked())
    markers->SetTrajectoryLength(25);
  else if (this->mp_ActionMarkerTrajectory50->isChecked())
    markers->SetTrajectoryLength(50);
  else if (this->mp_ActionMarkerTrajectory100->isChecked())
    markers->SetTrajectoryLength(100);
  else if (this->mp_ActionMarkerTrajectory200->isChecked())
    markers->SetTrajectoryLength(200);
    
  this->updateMarkersDisplay();
}

void MultiViewWidget::updateCameras()
{
  btk::VTKGroundSource* ground = btk::VTKGroundSource::SafeDownCast((*this->mp_VTKProc)[VTK_GROUND]);
  ground->Update();
  double n[3]; ground->GetNormal(n);
  for (QList<AbstractView*>::iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
  {
    Viz3DWidget* viz3d = static_cast<Viz3DWidget*>(static_cast<CompositeView*>(*it)->view(CompositeView::Viz3D));
    static_cast<btk::VTKInteractorStyleTrackballFixedUpCamera*>(viz3d->GetRenderWindow()->GetInteractor()->GetInteractorStyle())->SetGlobalUp(n);
  }
};

void MultiViewWidget::updateViews()
{
  btk::VTKMarkersFramesSource::SafeDownCast((*this->mp_VTKProc)[VTK_MARKERS])->Update(); // To update the markers' list when only charts are displayed.
  for (QList<AbstractView*>::const_iterator it = this->m_Views.begin() ; it != this->m_Views.end() ; ++it)
    static_cast<CompositeView*>(*it)->render();
};