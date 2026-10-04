/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "MetalPipelineStateViewer.h"
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QXmlStreamWriter>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/CollapseGroupBox.h"
#include "Widgets/Extended/RDHeaderView.h"
#include "Widgets/Extended/RDLabel.h"
#include "Widgets/Extended/RDTreeWidget.h"
#include "Widgets/PipelineFlowChart.h"
#include "PipelineStateViewer.h"

static const int MetalResourceIdRole = Qt::UserRole + 1;
static const int MetalBufferOffsetRole = Qt::UserRole + 2;
static const int MetalBufferSizeRole = Qt::UserRole + 3;
static const int MetalBufferSlotRole = Qt::UserRole + 4;
static const uint32_t MetalSamplerDescriptorOffset = 0x100;
static const uint32_t MetalBufferDescriptorOffset = 0x200;
static const uint32_t MetalComputeReadDescriptorOffset = 0x300;
static const uint32_t MetalComputeWriteDescriptorOffset = 0x400;
static const uint32_t MetalArgumentTextureDescriptorOffset = 0x500;
static const uint32_t MetalArgumentSamplerDescriptorOffset = 0x900;
static const uint32_t MetalComputeReadBufferDescriptorOffset = 0xB00;
static const uint32_t MetalComputeWriteBufferDescriptorOffset = 0xC00;
static const uint32_t MetalVertexTextureDescriptorOffset = 0xD00;
static const uint32_t MetalVertexSamplerDescriptorOffset = 0xE00;
static const uint32_t MetalVertexBufferDescriptorOffset = 0xF00;
static const uint32_t MetalComputeSamplerDescriptorOffset = 0x1000;

static uint32_t MetalDescriptorSlot(const DescriptorAccess &access)
{
  if(access.index == DescriptorAccess::NoShaderBinding && access.byteSize == 24)
    return access.arrayElement;
  if(access.byteOffset >= 0x10000 && access.byteOffset < 0x38000)
    return access.byteOffset & 0xff;
  // Argument-buffer member descriptors use a separate address space, not direct FS slots.
  if(access.stage == ShaderStage::Fragment && access.type == DescriptorType::Buffer &&
     access.byteOffset >= 0x2000 && access.byteOffset < 0x2000 + 32 * 32)
    return (access.byteOffset - 0x2000) % 32;
  if(access.stage == ShaderStage::Compute && access.type == DescriptorType::Sampler &&
     access.byteOffset >= MetalComputeSamplerDescriptorOffset)
    return access.byteOffset - MetalComputeSamplerDescriptorOffset;
  if(access.stage == ShaderStage::Compute && access.type == DescriptorType::Buffer &&
     access.byteOffset >= MetalComputeReadBufferDescriptorOffset)
    return access.byteOffset - MetalComputeReadBufferDescriptorOffset;
  if(access.stage == ShaderStage::Compute && access.type == DescriptorType::ReadWriteBuffer &&
     access.byteOffset >= MetalComputeWriteBufferDescriptorOffset)
    return access.byteOffset - MetalComputeWriteBufferDescriptorOffset;
  if(access.stage == ShaderStage::Vertex && access.type == DescriptorType::Buffer &&
     access.byteOffset >= MetalVertexBufferDescriptorOffset)
    return access.byteOffset - MetalVertexBufferDescriptorOffset;
  if(access.stage == ShaderStage::Vertex && access.type == DescriptorType::Sampler &&
     access.byteOffset >= MetalVertexSamplerDescriptorOffset)
    return access.byteOffset - MetalVertexSamplerDescriptorOffset;
  if(access.stage == ShaderStage::Vertex && access.type == DescriptorType::Image &&
     access.byteOffset >= MetalVertexTextureDescriptorOffset)
    return access.byteOffset - MetalVertexTextureDescriptorOffset;
  if(access.type == DescriptorType::Sampler &&
     access.byteOffset >= MetalArgumentSamplerDescriptorOffset)
    return (access.byteOffset - MetalArgumentSamplerDescriptorOffset) % 32;
  if(access.type == DescriptorType::Sampler && access.byteOffset >= MetalSamplerDescriptorOffset)
    return access.byteOffset - MetalSamplerDescriptorOffset;
  if((access.type == DescriptorType::ConstantBuffer || access.type == DescriptorType::Buffer ||
      access.type == DescriptorType::ReadWriteBuffer) &&
     access.byteOffset >= MetalBufferDescriptorOffset)
    return access.byteOffset - MetalBufferDescriptorOffset;
  if(access.stage == ShaderStage::Compute && access.type == DescriptorType::Image &&
     access.byteOffset >= MetalComputeReadDescriptorOffset)
    return access.byteOffset - MetalComputeReadDescriptorOffset;
  if(access.stage == ShaderStage::Compute && access.type == DescriptorType::ReadWriteImage &&
     access.byteOffset >= MetalComputeWriteDescriptorOffset)
    return access.byteOffset - MetalComputeWriteDescriptorOffset;
  if(access.stage == ShaderStage::Fragment && access.type == DescriptorType::Image &&
     access.byteOffset >= MetalArgumentTextureDescriptorOffset)
    return (access.byteOffset - MetalArgumentTextureDescriptorOffset) % 32;
  return access.byteOffset;
}

static const ShaderStage MetalUIStages[] = {ShaderStage::Vertex, ShaderStage::Fragment,
                                            ShaderStage::Compute, ShaderStage::Task,
                                            ShaderStage::Mesh, ShaderStage::Compute};
static const int MetalConstantStageRole = Qt::UserRole + 5;
static const int MetalConstantIndexRole = Qt::UserRole + 6;
static const int MetalConstantArrayRole = Qt::UserRole + 7;

MetalPipelineStateViewer::MetalPipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), m_Ctx(ctx)
{
  setObjectName(lit("MetalPipelineStateViewer"));
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  QFrame *toolbar = new QFrame(this);
  toolbar->setFrameShape(QFrame::Panel);
  toolbar->setFrameShadow(QFrame::Raised);
  QHBoxLayout *controls = new QHBoxLayout(toolbar);
  controls->setContentsMargins(6, 2, 6, 2);
  controls->addWidget(new QLabel(tr("Controls"), toolbar));
  QFrame *separator = new QFrame(toolbar);
  separator->setFrameShape(QFrame::VLine);
  controls->addWidget(separator);
  auto button = [controls, toolbar](const QString &text, const QIcon &icon) {
    QToolButton *b = new QToolButton(toolbar);
    b->setText(text);
    b->setIcon(icon);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setAutoRaise(true);
    controls->addWidget(b);
    return b;
  };
  m_ShowUnused = button(tr("Show Unused Items"), QIcon(lit(":/page_white_delete.png")));
  m_ShowUnused->setObjectName(lit("metalShowUnused"));
  m_ShowUnused->setCheckable(true);
  m_ShowEmpty = button(tr("Show Empty Items"), QIcon(lit(":/page_white_database.png")));
  m_ShowEmpty->setCheckable(true);
  m_Export = button(tr("Export"), Icons::save());
  m_Export->setAccessibleName(tr("Export Pipeline State"));
  QToolButton *extensions = button(tr("Extensions"), Icons::plugin());
  QMenu *extensionsMenu = new QMenu(extensions);
  extensions->setMenu(extensionsMenu);
  extensions->setPopupMode(QToolButton::InstantPopup);
  QObject::connect(extensionsMenu, &QMenu::aboutToShow, this, [this, extensionsMenu, extensions]() {
    extensionsMenu->clear();
    m_Ctx.Extensions().MenuDisplaying(PanelMenu::PipelineStateViewer, extensionsMenu, extensions, {});
  });
  controls->addStretch();
  layout->addWidget(toolbar);
  m_PipeFlow = new PipelineFlowChart(this);
  QFont flowFont = m_PipeFlow->font();
  flowFont.setPointSize(12);
  m_PipeFlow->setFont(flowFont);
  layout->addWidget(m_PipeFlow);
  m_Stages = new QTabWidget(this);
  m_Stages->setDocumentMode(true);
  layout->addWidget(m_Stages);

  auto horizontal = [](QVBoxLayout *parent) {
    QWidget *row = new QWidget;
    QHBoxLayout *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    parent->addWidget(row, 1);
    return h;
  };
  auto column = [](QHBoxLayout *row, int stretch) {
    QWidget *w = new QWidget;
    QVBoxLayout *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    row->addWidget(w, stretch);
    return v;
  };
  QVBoxLayout *ia = MakeStagePage(tr("Input Assembly"));
  m_VertexAttributes =
      MakeTree(ia, tr("Input Layouts"),
               {tr("Slot"), tr("Semantic"), tr("Index"), tr("Format"), tr("Input Slot"),
                tr("Offset"), tr("Class"), tr("Step Rate"), tr("Go")});
  QHBoxLayout *inputRow = horizontal(ia);
  QVBoxLayout *buffers = column(inputRow, 5);
  m_VertexBuffers =
      MakeTree(buffers, tr("Buffers"),
               {tr("Slot"), tr("Buffer"), tr("Stride"), tr("Offset"), tr("Byte Length"), tr("Go")});
  m_IndirectBuffer =
      MakeTree(buffers, tr("Indirect Draw"),
               {tr("Buffer"), tr("Offset"), tr("Byte Length"), tr("Arguments"), tr("Go")});
  m_IndirectBuffer->parentWidget()->setVisible(false);
  QVBoxLayout *mesh = column(inputRow, 1);
  QGroupBox *meshGroup = new QGroupBox(tr("Mesh View"));
  QVBoxLayout *meshLayout = new QVBoxLayout(meshGroup);
  m_MeshView = new RDLabel(meshGroup);
  m_MeshView->setAlignment(Qt::AlignCenter);
  meshLayout->addWidget(m_MeshView);
  mesh->addWidget(meshGroup);
  PipelineStateViewer *common = static_cast<PipelineStateViewer *>(parentWidget());
  common->setMeshViewPixmap(m_MeshView);
  QObject::connect(m_MeshView, &RDLabel::clicked, this, [this](QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
      m_Ctx.ShowMeshPreview();
  });
  QVBoxLayout *topology = column(inputRow, 2);
  QGroupBox *topologyGroup = new QGroupBox(tr("Primitive Topology"));
  QVBoxLayout *topologyLayout = new QVBoxLayout(topologyGroup);
  m_Topology = new QLabel;
  m_Topology->setAlignment(Qt::AlignCenter);
  m_TopologyDiagram = new QLabel;
  m_TopologyDiagram->setAlignment(Qt::AlignCenter);
  topologyLayout->addWidget(m_Topology);
  topologyLayout->addWidget(m_TopologyDiagram, 1);
  QLabel *restart = new QLabel(tr("Restart Idx: Disabled"));
  restart->setAlignment(Qt::AlignCenter);
  topologyLayout->addWidget(restart);
  topology->addWidget(topologyGroup);

  m_Shaders[0] = MakeShader(MakeStagePage(tr("Vertex Shader")), ShaderStage::Vertex);
  QVBoxLayout *tess = MakeStagePage(tr("Tessellator"));
  QLabel *tessInfo =
      new QLabel(tr("Metal tessellation factors are generated by a separate compute dispatch. "
                    "The post-tessellation vertex function is shown in TES; select its producing "
                    "dispatch to inspect CS."));
  tessInfo->setWordWrap(true);
  tess->addWidget(tessInfo);
  m_Tessellation = MakeSummary(tess, tr("Tessellation State"),
                               {tr("Control Points"), tr("Partition Mode"), tr("Max Factor"),
                                tr("Factor Format"), tr("Step Function"), tr("Output Winding"),
                                tr("Factor Scaling"), tr("Instance Stride"), tr("Factor Scale")},
                               2);
  m_TessellationBuffer = MakeTree(tess, tr("Tessellation Factors"),
                                  {tr("Buffer"), tr("Offset"), tr("Byte Length"), tr("Go")});
  QVBoxLayout *rs = MakeStagePage(tr("Rasterizer"));
  m_Raster = MakeSummary(
      rs, tr("Rasterizer State"),
      {tr("Fill Mode"), tr("Cull Mode"), tr("Front CCW"), tr("Depth Bias"), tr("Depth Bias Clamp"),
       tr("Slope-Scaled Bias"), tr("Depth Clip"), tr("Rasterization Enabled"), tr("Samples")},
      3);
  m_VRR = MakeSummary(rs, tr("Variable Rasterization Rate"),
                      {tr("Enabled"), tr("Logical Screen Size"), tr("Layers")}, 3);
  m_VRR[0]->parentWidget()->setObjectName(lit("metalVRRState"));
  m_VRRMap = MakeTree(rs, tr("Rasterization Rate Map"), {tr("Resource"), tr("Go")});
  m_VRRLayers = MakeTree(rs, tr("Rasterization Rate Layers"),
                       {tr("Layer"), tr("Physical Size"), tr("Horizontal Rates"), tr("Vertical Rates")});
  QHBoxLayout *rasterRow = horizontal(rs);
  m_Viewports = MakeTree(
      column(rasterRow, 1), tr("Viewports"),
      {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height"), tr("MinDepth"), tr("MaxDepth")});
  m_Scissors = MakeTree(column(rasterRow, 1), tr("Scissor Regions"),
                        {tr("Slot"), tr("X"), tr("Y"), tr("Width"), tr("Height")});
  m_Shaders[1] = MakeShader(MakeStagePage(tr("Fragment Shader")), ShaderStage::Fragment);
  QVBoxLayout *om = MakeStagePage(tr("Output Merger"));
  m_Targets = MakeTree(om, tr("Render Targets"),
                       {tr("Slot"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                        tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
  m_ResolveTargets = MakeTree(om, tr("Resolve Targets"), m_Targets->getHeaders());
  m_ResolveTargets->parentWidget()->setVisible(false);
  m_ColorBlends = MakeTree(om, tr("Target Blends"),
                           {tr("Slot"), tr("Enabled"), tr("Col Src"), tr("Col Dst"), tr("Col Op"),
                            tr("Alpha Src"), tr("Alpha Dst"), tr("Alpha Op"), tr("Write Mask")});
  m_Pass = MakeSummary(om, tr("Render Pass"),
                       {tr("Imageblock Sample Length"), tr("Threadgroup Memory Length")}, 2);
  m_AttachmentActions = MakeTree(om, tr("Attachment Operations"),
                                {tr("Attachment"), tr("Resource"), tr("Storage"), tr("Load"),
                                 tr("Store"), tr("Store Options"), tr("Go")});
  QHBoxLayout *outputRow = horizontal(om);
  om->setStretch(om->count() - 1, 0);
  outputRow->parentWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  m_Blend = MakeSummary(column(outputRow, 2), tr("Blend State"),
                        {tr("Alpha to Coverage"), tr("Alpha to One"), tr("Blend Factor")}, 1);
  m_Blend[0]->parentWidget()->setToolTip(
      tr("Fixed-function blending runs after fragment shading. Framebuffer fetch and any "
         "programmable blend calculation are part of the fragment shader; both can be used together."));
  m_Depth = MakeSummary(column(outputRow, 1), tr("Depth State"),
                        {tr("Enabled"), tr("Func"), tr("Write")}, 1);
  m_Stencil = MakeTree(column(outputRow, 4), tr("Stencil State"),
                       {tr("Face"), tr("Func"), tr("Fail Op"), tr("Depth Fail Op"), tr("Pass Op"),
                        tr("Write Mask"), tr("Comp Mask"), tr("Ref")});
  m_Stencil->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
  QVBoxLayout *cs = MakeStagePage(tr("Compute Shader"));
  m_Shaders[2] = MakeShader(cs, ShaderStage::Compute);
  m_ComputeIndirect =
      MakeTree(cs, tr("Indirect Dispatch"),
               {tr("Buffer"), tr("Offset"), tr("Byte Length"), tr("Arguments"), tr("Go")});
  m_ComputeIndirect->parentWidget()->setVisible(false);
  m_Shaders[3] = MakeShader(MakeStagePage(tr("Object / Task Shader")), ShaderStage::Task);
  m_Shaders[4] = MakeShader(MakeStagePage(tr("Mesh Shader")), ShaderStage::Mesh);
  QVBoxLayout *tile = MakeStagePage(tr("Tile Shader"));
  m_Shaders[5] = MakeShader(tile, ShaderStage::Compute);
  m_Shaders[5].view->setAccessibleName(tr("View Tile Shader"));
  m_Shaders[5].save->setAccessibleName(tr("Save Tile Shader"));
  m_Shaders[5].edit->setAccessibleName(tr("Edit Tile Shader"));
  auto tileInfo = new QLabel(tr("Tile kernels execute inside the current render pass. Bindings "
                               "below are tile bindings; tile memory is shared within this pass."));
  tileInfo->setWordWrap(true);
  tile->addWidget(tileInfo);
  m_Tile = MakeSummary(tile, tr("Tile Dispatch && Render Pass"),
                      {tr("Tile Size"), tr("Threads per Tile"), tr("Max Threads per Tile"),
                       tr("Threadgroup Matches Tile"), tr("Imageblock Sample Length"),
                       tr("Threadgroup Memory Length"), tr("Raster Samples")}, 2);
  m_TileMemory = MakeTree(tile, tr("Tile Threadgroup Memory"),
                          {tr("Slot"), tr("Offset"), tr("Length")});
  m_TileTargets = MakeTree(tile, tr("Render Pass Attachments"),
                           {tr("Attachment"), tr("Resource"), tr("Storage"), tr("Load"),
                            tr("Store"), tr("Store Options"), tr("Go")});
  // Keep dispatch parameters near the shader header, before potentially long binding tables.
  tile->insertWidget(0, tileInfo);
  tile->insertWidget(1, m_Tile[0]->parentWidget());
  QWidget *tileFeatures = m_Shaders[5].features[0]->parentWidget();
  tile->removeWidget(tileFeatures);
  tile->addWidget(tileFeatures);
  QVBoxLayout *fx = MakeStagePage(tr("MetalFX Upscaler"));
  auto fxInfo = new QLabel(tr("This operation replays through MetalFX. Internal framework shaders "
                             "are opaque; shader editing and single-step debugging are unavailable."));
  fxInfo->setWordWrap(true);
  fx->addWidget(fxInfo);
  m_FX = MakeSummary(fx, tr("Spatial Upscaler"),
                    {tr("Input Size"), tr("Output Size"), tr("Input Content Size"),
                     tr("Color Processing Mode"), tr("Input Format"), tr("Output Format")}, 2);
  m_FXTemporal = MakeSummary(fx, tr("Temporal Upscaler"),
       {tr("Scaler"), tr("Jitter Offset"), tr("Motion Vector Scale"), tr("Pre-Exposure"),
        tr("Reset History"), tr("Reversed Depth"), tr("Auto Exposure"), tr("Dynamic Resolution"),
        tr("Content Scale Range"), tr("Reactive Mask"), tr("Synchronous Initialization"), tr("Replay History")}, 2);
  m_FXTemporal[0]->parentWidget()->setObjectName(lit("metalFXTemporalState"));
  m_FXTemporal[11]->setWordWrap(true);
  m_FXResources = MakeTree(fx, tr("Textures"),
                           {tr("Binding"), tr("Resource"), tr("Type"), tr("Width"), tr("Height"),
                            tr("Depth"), tr("Array Size"), tr("Format"), tr("Go")});
  m_Stages->tabBar()->hide();
  QObject::connect(m_PipeFlow, &PipelineFlowChart::stageSelected, this, [this](int index) {
    if(index < m_FlowPages.size())
      m_Stages->setCurrentIndex(m_FlowPages[index]);
  });
  SetFlow(false, false);
  QObject::connect(m_ShowEmpty, &QToolButton::toggled, this, [this](bool) { SetState(); });
  QObject::connect(m_ShowUnused, &QToolButton::toggled, this, [this](bool) { SetState(); });
  QObject::connect(m_Export, &QToolButton::clicked, this, &MetalPipelineStateViewer::ExportHTML);
}

QList<QLabel *> MetalPipelineStateViewer::MakeSummary(QVBoxLayout *parent, const QString &title,
                                                      const QStringList &labels, int columns)
{
  QGroupBox *group = new QGroupBox(title, this);
  QGridLayout *grid = new QGridLayout(group);
  grid->setSpacing(4);
  QList<QLabel *> values;
  for(int i = 0; i < labels.size(); i++)
  {
    QLabel *name = new QLabel(labels[i] + lit(":"));
    name->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QLabel *value = new QLabel;
    value->setFont(Formatter::PreferredFont());
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setAccessibleName(labels[i]);
    grid->addWidget(name, i / columns, 2 * (i % columns));
    grid->addWidget(value, i / columns, 2 * (i % columns) + 1);
    grid->setColumnStretch(2 * (i % columns) + 1, 1);
    values << value;
  }
  addGridLines(grid, palette().color(QPalette::WindowText));
  parent->addWidget(group);
  return values;
}

MetalPipelineStateViewer::ShaderWidgets MetalPipelineStateViewer::MakeShader(QVBoxLayout *layout,
                                                                             ShaderStage stage)
{
  QGroupBox *group = new QGroupBox(tr("Pipeline && Shader"), this);
  FlowLayout *flow = new FlowLayout(group, -1, 3, 3);
  ShaderWidgets w;
  auto resourceLabel = [group]() {
    RDLabel *label = new RDLabel(group);
    label->setAutoFillBackground(true);
    label->setBackgroundRole(QPalette::ToolTipBase);
    label->setForegroundRole(QPalette::ToolTipText);
    label->setFont(Formatter::PreferredFont());
    label->setMinimumSizeHint(QSize(180, 0));
    label->setFrameShape(QFrame::Box);
    label->setMargin(4);
    return label;
  };
  w.pipeline = resourceLabel();
  flow->addWidget(w.pipeline);
  QToolButton *pipelineView = new QToolButton(group);
  pipelineView->setText(tr("View"));
  pipelineView->setIcon(QIcon(lit(":/action.png")));
  pipelineView->setAutoRaise(true);
  pipelineView->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  flow->addWidget(pipelineView);
  auto inspect = [this, stage]() {
    const PipeState &pipe = m_Ctx.CurPipelineState();
    const ResourceId id = stage == ShaderStage::Compute ? pipe.GetComputePipelineObject()
                                                        : pipe.GetGraphicsPipelineObject();
    if(id != ResourceId())
    {
      m_Ctx.ShowResourceInspector();
      m_Ctx.GetResourceInspector()->Inspect(id);
    }
  };
  QObject::connect(pipelineView, &QToolButton::clicked, this, inspect);
  QObject::connect(w.pipeline, &RDLabel::clicked, this, [inspect](QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
      inspect();
  });
  w.resource = resourceLabel();
  flow->addWidget(w.resource);
  w.entryPoint = new QLabel(group);
  w.entryPoint->setFont(Formatter::PreferredFont());
  flow->addWidget(w.entryPoint);
  auto button = [group, flow](const QString &text, const QIcon &icon) {
    QToolButton *b = new QToolButton(group);
    b->setText(text);
    b->setIcon(icon);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setAutoRaise(true);
    flow->addWidget(b);
    return b;
  };
  w.view = button(tr("View"), QIcon(lit(":/action.png")));
  w.edit = button(tr("Edit"), Icons::page_white_edit());
  w.edit->setPopupMode(QToolButton::MenuButtonPopup);
  w.edit->setEnabled(false);
  w.edit->setAccessibleName(tr("Edit %1").arg(ToQStr(stage, GraphicsAPI::Metal)));
  QObject::connect(w.edit, &QToolButton::clicked,
                   static_cast<PipelineStateViewer *>(parentWidget()),
                   &PipelineStateViewer::shaderEdit_clicked);
  w.save = button(tr("Save"), Icons::save());
  w.view->setAccessibleName(tr("View %1").arg(ToQStr(stage, GraphicsAPI::Metal)));
  w.save->setAccessibleName(tr("Save %1").arg(ToQStr(stage, GraphicsAPI::Metal)));
  w.save->setToolTip(
      tr("Save captured shader binary (compiled shaders contain the full metallib)"));
  // Match D3D12/Vulkan: keep the wrapping shader header outside the resource scroll area.
  // Otherwise QScrollArea's height-for-width calculation treats every tree's sizeHint as a
  // minimum, forcing the last resource groups off-screen even on a large window.
  auto pageLayout = static_cast<QVBoxLayout *>(m_Stages->widget(m_Stages->count() - 1)->layout());
  pageLayout->insertWidget(0, group);
  QObject::connect(w.resource, &RDLabel::clicked, this, [this, stage](QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
      ViewShader(stage);
  });
  QObject::connect(w.view, &QToolButton::clicked, this, [this, stage]() { ViewShader(stage); });
  QObject::connect(w.save, &QToolButton::clicked, this, [this, stage]() {
    static_cast<PipelineStateViewer *>(parentWidget())
        ->SaveShaderFile(m_Ctx.CurPipelineState().GetShaderReflection(stage));
  });
  const QStringList resourceHeaders = {tr("Binding"),    tr("Resource"), tr("Type"),
                                       tr("Width"),      tr("Height"),   tr("Depth"),
                                       tr("Array Size"), tr("Format"),   tr("Go")};
  w.resources = MakeTree(layout, tr("Resources"), resourceHeaders);
  w.uavs = MakeTree(layout, tr("UAVs"), resourceHeaders);
  w.samplers =
      MakeTree(layout, tr("Samplers"),
               {tr("Binding"), tr("Addressing"), tr("Filter"), tr("LOD Clamp"), tr("LOD Bias")});
  w.constants = MakeTree(layout, tr("Constant Buffers"),
                         {tr("Binding"), tr("Buffer"), tr("Byte Range"), tr("Size"), tr("Go")});
  w.features = MakeSummary(layout, tr("Metal Shader Features"),
                           {tr("Framebuffer Fetch"), tr("Raster Order Groups"),
                            tr("Imageblock"), tr("Metadata Source")}, 2);
  w.features[0]->parentWidget()->setObjectName(lit("metalShaderFeatures"));
  w.features[0]->parentWidget()->setToolTip(
      tr("Shader attachment access and ordering declarations. Framebuffer fetch may be used "
         "for programmable blending or other shader calculations. Show Unused Items also "
         "shows inactive declarations and unavailable metadata."));
  return w;
}

void MetalPipelineStateViewer::SetShader(ShaderWidgets &w, ShaderStage stage, bool bound)
{
  const PipeState &pipe = m_Ctx.CurPipelineState();
  const ResourceId id = bound ? pipe.GetShader(stage) : ResourceId();
  const ResourceId pipeline =
      bound ? (stage == ShaderStage::Compute ? pipe.GetComputePipelineObject()
                                             : pipe.GetGraphicsPipelineObject())
            : ResourceId();
  const ShaderReflection *reflection = bound ? pipe.GetShaderReflection(stage) : NULL;
  w.pipeline->setText(pipeline == ResourceId() ? tr("No Pipeline") : ToQStr(pipeline));
  w.resource->setText(id == ResourceId() ? tr("Unbound") : ToQStr(id));
  QString entry =
      id != ResourceId() ? QString(pipe.GetShaderEntryPoint(stage)) + lit("()") : QString();
  w.entryPoint->setToolTip(entry);
  TruncateStringFromEnd(entry);
  w.entryPoint->setText(entry);
  w.entryPoint->setVisible(!entry.isEmpty());
  w.view->setEnabled(reflection != NULL);
  w.save->setEnabled(reflection && !reflection->rawBytes.empty());
  w.edit->setEnabled(reflection != NULL);
  static_cast<PipelineStateViewer *>(parentWidget())
      ->SetupShaderEditButton(w.edit, pipeline, id, reflection);
}

void MetalPipelineStateViewer::ViewShader(ShaderStage stage)
{
  const PipeState &pipe = m_Ctx.CurPipelineState();
  const ShaderReflection *reflection = pipe.GetShaderReflection(stage);
  if(!reflection)
    return;
  auto viewer = m_Ctx.ViewShader(reflection, stage == ShaderStage::Compute
                                                 ? pipe.GetComputePipelineObject()
                                                 : pipe.GetGraphicsPipelineObject());
  m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
}

QVBoxLayout *MetalPipelineStateViewer::MakeStagePage(const QString &title)
{
  QWidget *page = new QWidget(m_Stages);
  QVBoxLayout *pageLayout = new QVBoxLayout(page);
  pageLayout->setContentsMargins(6, 6, 6, 6);
  QScrollArea *scroll = new QScrollArea(page);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setAccessibleName(title);
  QWidget *contents = new QWidget(scroll);
  contents->setObjectName(lit("metalStageContents%1").arg(m_Stages->count()));
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  scroll->setWidget(contents);
  pageLayout->addWidget(scroll, 1);
  m_Stages->addTab(page, title);
  return layout;
}

RDTreeWidget *MetalPipelineStateViewer::MakeTree(QVBoxLayout *layout, const QString &title,
                                                 const QStringList &headers)
{
  CollapseGroupBox *group = new CollapseGroupBox(this);
  group->setTitle(title);
  group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  QVBoxLayout *groupLayout = new QVBoxLayout(group);
  groupLayout->setContentsMargins(4, 4, 4, 4);
  RDTreeWidget *tree = new RDTreeWidget(group);
  tree->setAccessibleName(title);
  tree->setObjectName(lit("metal") + title);
  RDHeaderView *header = new RDHeaderView(Qt::Horizontal, tree);
  tree->setHeader(header);
  tree->setColumns(headers);
  QList<int> hints;
  for(const QString &h : headers)
    hints << (h == tr("Go")                              ? -1
              : h == tr("Resource") || h == tr("Buffer") ? 4
              : h == tr("Format")                        ? 3
                                                         : 1);
  if(headers.contains(tr("Byte Range")))
    hints = {2, 4, 2, 4, -1};
  header->setColumnStretchHints(hints);
  tree->setFrameShape(QFrame::Box);
  tree->setFrameShadow(QFrame::Plain);
  tree->setUniformRowHeights(true);
  tree->setRootIsDecorated(false);
  tree->setItemsExpandable(false);
  tree->setAllColumnsShowFocus(true);
  tree->setFont(Formatter::PreferredFont());
  tree->setClearSelectionOnFocusLoss(true);
  tree->setInstantTooltips(true);
  if(headers.last() == tr("Go"))
    tree->setHoverIconColumn(headers.size() - 1, Pixmaps::action(this), Pixmaps::action_hover(this));
  tree->setMinimumHeight(90);
  groupLayout->addWidget(tree);
  layout->addWidget(group, 1);
  static_cast<PipelineStateViewer *>(parentWidget())->SetupResourceView(tree);
  QObject::connect(tree, &RDTreeWidget::itemActivated, this, [this, tree](RDTreeWidgetItem *item, int) {
    if(tree == m_VertexAttributes)
    {
      m_Ctx.ShowMeshPreview();
      return;
    }
    if(item->data(0, MetalConstantStageRole).isValid())
    {
      IBufferViewer *viewer =
          m_Ctx.ViewConstantBuffer(ShaderStage(item->data(0, MetalConstantStageRole).toUInt()),
                                   item->data(0, MetalConstantIndexRole).toUInt(),
                                   item->data(0, MetalConstantArrayRole).toUInt());
      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
      return;
    }
    const ResourceId id = GetResource(item);
    if(id == ResourceId())
      return;
    if(m_Ctx.GetBuffer(id))
    {
      QString format;
      if(tree == m_VertexBuffers)
      {
        const int slot = item->data(0, MetalBufferSlotRole).toInt();
        format = slot < 0
                     ? (item->data(0, MetalBufferSizeRole + 10).toUInt() == 2 ? lit("ushort index;")
                                                                              : lit("uint index;"))
                     : static_cast<PipelineStateViewer *>(parentWidget())
                           ->GetVBufferFormatString(uint32_t(slot));
      }
      else if(tree == m_ComputeIndirect)
        format = lit("uint x; uint y; uint z;");
      auto viewer = m_Ctx.ViewBuffer(item->data(0, MetalBufferOffsetRole).toULongLong(),
                                     item->data(0, MetalBufferSizeRole).toULongLong(), id, format);
      m_Ctx.AddDockWindow(viewer->Widget(), DockReference::AddTo, this);
    }
    else if(m_Ctx.GetTexture(id))
    {
      if(!m_Ctx.HasTextureViewer())
        m_Ctx.ShowTextureViewer();
      m_Ctx.GetTextureViewer()->ViewTexture(id, CompType::Typeless, true);
    }
    else
    {
      m_Ctx.ShowResourceInspector();
      m_Ctx.GetResourceInspector()->Inspect(id);
    }
  });
  return tree;
}

RDTreeWidgetItem *MetalPipelineStateViewer::AddResourceRow(RDTreeWidget *tree,
                                                           const QStringList &values,
                                                           ResourceId resource)
{
  QVariantList columns;
  for(const QString &v : values)
    columns << v;
  RDTreeWidgetItem *item = new RDTreeWidgetItem(columns);
  item->setTag(QVariant::fromValue(resource));
  item->setData(0, MetalResourceIdRole, QVariant::fromValue(resource));
  tree->addTopLevelItem(item);
  return item;
}
RDTreeWidgetItem *MetalPipelineStateViewer::AddEmptyRow(RDTreeWidget *tree, const QStringList &values)
{
  auto item = AddResourceRow(tree, values, ResourceId());
  item->setBackgroundColor(QColor(255, 70, 70));
  item->setForegroundColor(QColor(0, 0, 0));
  return item;
}
ResourceId MetalPipelineStateViewer::GetResource(RDTreeWidgetItem *item)
{
  return item && item->tag().canConvert<ResourceId>() ? item->tag().value<ResourceId>()
                                                      : ResourceId();
}

RDTreeWidgetItem *MetalPipelineStateViewer::AddDescriptor(RDTreeWidget *tree, const QString &binding,
                                                          const Descriptor &d)
{
  QStringList columns = {binding, d.resource == ResourceId() ? tr("Empty") : ToQStr(d.resource)};
  auto texture = m_Ctx.GetTexture(d.resource);
  auto buffer = m_Ctx.GetBuffer(d.resource);
  if(texture)
    columns << ToQStr(texture->type) << Formatter::Format(texture->width)
            << Formatter::Format(texture->height) << Formatter::Format(texture->depth)
            << Formatter::Format(texture->arraysize) << QString(d.format.Name());
  else if(buffer)
    columns << tr("Buffer") << Formatter::HumanFormat(d.byteSize, Formatter::OffsetSize)
            << QString() << QString() << QString() << QString(d.format.Name());
  else
    columns << (d.type == DescriptorType::AccelerationStructure ? tr("Acceleration Structure")
                                                                : QString())
            << QString() << QString() << QString() << QString() << QString();
  const bool viewFormat = texture && d.format != texture->format;
  if(viewFormat)
    columns[7] = tr("Viewed as %1").arg(columns[7]);
  columns << QString();
  auto item = d.resource == ResourceId() ? AddEmptyRow(tree, columns)
                                         : AddResourceRow(tree, columns, d.resource);
  item->setData(0, MetalBufferOffsetRole, qulonglong(d.byteOffset));
  item->setData(0, MetalBufferSizeRole, qulonglong(d.byteSize));
  if(viewFormat)
    item->setBackgroundColor(
        static_cast<PipelineStateViewer *>(parentWidget())->GetViewDetailsColor());
  if(texture)
    item->setToolTip(
        tr("Mip %1, slice %2, samples %3").arg(d.firstMip).arg(d.firstSlice).arg(texture->msSamp));
  else if(buffer)
    item->setToolTip(tr("Byte range %1 - %2").arg(d.byteOffset).arg(d.byteOffset + d.byteSize));
  return item;
}

void MetalPipelineStateViewer::SetFlow(bool mesh, bool tess, bool tile, bool metalFX)
{
  QStringList abbrevs, names;
  if(metalFX)
  {
    m_FlowPages = {10};
    abbrevs = QStringList({tr("MetalFX")});
    names = QStringList({tr("MetalFX Upscaler")});
  }
  else if(tile)
  {
    m_FlowPages = {9};
    abbrevs = QStringList({tr("Tile")});
    names = QStringList({tr("Tile Shader")});
  }
  else if(mesh)
  {
    m_FlowPages = {7, 8, 3, 4, 5, 6};
    abbrevs = QStringList({lit("TS"), lit("MS"), tr("Rasterizer"), lit("FS"), lit("OM"), lit("CS")});
    names = QStringList({tr("Object / Task Shader"), tr("Mesh Shader"), tr("Rasterizer"),
                         tr("Fragment Shader"), tr("Output Merger"), tr("Compute Shader")});
  }
  else
  {
    m_FlowPages = tess ? QList<int>({0, 2, 1, 3, 4, 5, 6}) : QList<int>({0, 1, 2, 3, 4, 5, 6});
    abbrevs = tess ? QStringList({lit("IA"), tr("Tessellator"), lit("TES"), tr("Rasterizer"),
                                  lit("FS"), lit("OM"), lit("CS")})
                   : QStringList({lit("IA"), lit("VS"), tr("Tessellator"), tr("Rasterizer"),
                                  lit("FS"), lit("OM"), lit("CS")});
    names = tess ? QStringList({tr("Input Assembly"), tr("Tessellator"),
                                tr("Post-Tessellation Vertex Shader"), tr("Rasterizer"),
                                tr("Fragment Shader"), tr("Output Merger"), tr("Compute Shader")})
                 : QStringList({tr("Input Assembly"), tr("Vertex Shader"), tr("Tessellator"),
                                tr("Rasterizer"), tr("Fragment Shader"), tr("Output Merger"),
                                tr("Compute Shader")});
  }
  // setStages resets the flow edges. Reapply isolation on every pipeline-path change.
  if(m_PipeFlow->stageAbbreviations() != abbrevs)
    m_PipeFlow->setStages(abbrevs, names);
  m_PipeFlow->setIsolatedStage(abbrevs.size() - 1);
}

void MetalPipelineStateViewer::ClearState()
{
  for(int i = 0; i < 6; i++)
  {
    SetShader(m_Shaders[i], MetalUIStages[i], false);
    for(auto label : m_Shaders[i].features) label->setText(tr("—"));
    m_Shaders[i].features[0]->parentWidget()->hide();
    for(auto tree :
        {m_Shaders[i].resources, m_Shaders[i].uavs, m_Shaders[i].samplers, m_Shaders[i].constants})
      tree->clear();
  }
  for(auto tree :
      {m_VertexAttributes, m_VertexBuffers, m_IndirectBuffer, m_Viewports, m_Scissors, m_Targets,
       m_ResolveTargets, m_ColorBlends, m_Stencil, m_TessellationBuffer, m_ComputeIndirect, m_TileMemory, m_TileTargets, m_AttachmentActions, m_FXResources, m_VRRMap, m_VRRLayers})
    tree->clear();
  for(const auto &list : {m_Raster, m_Blend, m_Depth, m_Tessellation, m_Tile, m_Pass, m_FX, m_FXTemporal, m_VRR})
    for(QLabel *label : list)
      label->setText(tr("—"));
  m_Topology->setText(ToQStr(Topology::Unknown));
  static_cast<PipelineStateViewer *>(parentWidget())
      ->setTopologyDiagram(m_TopologyDiagram, Topology::Unknown);
  m_ResolveTargets->parentWidget()->hide();
  m_IndirectBuffer->parentWidget()->hide();
  m_ComputeIndirect->parentWidget()->hide();
}

void MetalPipelineStateViewer::SetState()
{
  ClearState();
  const PipeState &pipe = m_Ctx.CurPipelineState();
  const MetalPipe::State *state = pipe.GetMetalPipelineState();
  if(!pipe.IsCaptureMetal() || !state)
  {
    SetFlow(false, false);
    m_PipeFlow->setStagesEnabled({false, false, false, false, false, false, false});
    return;
  }
  const bool compute = pipe.GetComputePipelineObject() != ResourceId();
  const bool mesh = pipe.GetShader(ShaderStage::Mesh) != ResourceId();
  const bool tess = state->patchControlPoints != 0;
  const bool tile = state->tileDispatch;
  const bool temporal = state->metalFXTemporal.size() == 17;
  const bool metalFX = state->metalFXSpatial.size() == 9 || temporal;
  SetFlow(mesh, tess, tile, metalFX);
  if(metalFX)
  {
    m_PipeFlow->setStagesEnabled({true});
    m_PipeFlow->setSelectedStage(0);
    m_FXTemporal[0]->parentWidget()->setVisible(temporal);
    auto group = qobject_cast<QGroupBox *>(m_FX[0]->parentWidget());
    if(group) group->setTitle(temporal ? tr("Temporal Upscaler Dimensions") : tr("Spatial Upscaler"));
    const auto &p = temporal ? state->metalFXTemporal : state->metalFXSpatial;
    m_FX[0]->setText(tr("%1 × %2").arg(p[0]).arg(p[1]));
    m_FX[1]->setText(tr("%1 × %2").arg(p[2]).arg(p[3]));
    m_FX[2]->setText(tr("%1 × %2").arg(p[temporal ? 10 : 7]).arg(p[temporal ? 11 : 8]));
    m_FX[3]->setText(temporal ? tr("Not applicable (Temporal)") :
                     p[6] == 0 ? tr("Perceptual") : p[6] == 1 ? tr("Linear") : tr("HDR"));
    if(temporal && state->metalFXTemporalFloats.size() == 7)
    {
      const auto &v = state->metalFXTemporalFloats;
      m_FXTemporal[0]->setText(ToQStr(state->metalFXScaler));
      m_FXTemporal[1]->setText(tr("%1, %2").arg(v[0]).arg(v[1]));
      m_FXTemporal[2]->setText(tr("%1, %2").arg(v[2]).arg(v[3]));
      m_FXTemporal[3]->setText(Formatter::Format(v[4]));
      for(auto pair : {qMakePair(4, 12), qMakePair(5, 13), qMakePair(6, 8), qMakePair(7, 9),
                       qMakePair(9, 14), qMakePair(10, 16)})
        m_FXTemporal[pair.first]->setText(p[pair.second] ? tr("Yes") : tr("No"));
      m_FXTemporal[8]->setText(tr("%1 – %2").arg(v[5]).arg(v[6]));
      m_FXTemporal[11]->setText(state->metalFXHistoryUnavailable ?
        tr("Pre-capture history unavailable: replay starts with reset; output may differ until a captured reset.") :
        tr("History reconstructed from captured calls"));
    }
    const QStringList names = {tr("Color"), tr("Output"), tr("Depth"), tr("Motion Vectors"),
                               tr("Exposure"), tr("Reactive Mask")};
    for(int i = 0; i < (temporal ? 6 : 2); i++)
    {
      auto id = i == 1 ? state->metalFXOutput : i == 0 ? state->metalFXInput : state->metalFXTemporalInputs[i-1];
      if(id == ResourceId()) continue;
      const auto texture = m_Ctx.GetTexture(id);
      if(i < 2) m_FX[4+i]->setText(texture ? QString(texture->format.Name()) : tr("Unavailable"));
      Descriptor descriptor;
      descriptor.type = i == 1 ? DescriptorType::ReadWriteImage : DescriptorType::Image;
      descriptor.resource = id;
      if(texture) { descriptor.format = texture->format; descriptor.textureType = texture->type; }
      AddDescriptor(m_FXResources, temporal ? names[i] : i ? tr("Output") : tr("Input"), descriptor);
    }
    return;
  }
  QList<bool> enabled;
  for(int page : m_FlowPages)
    enabled << (tile        ? page == 9
                : compute   ? page == 6
                : page == 6 ? false
                : page == 2 ? tess
                : page == 1 ? pipe.GetShader(ShaderStage::Vertex) != ResourceId()
                : page == 4 ? pipe.GetShader(ShaderStage::Fragment) != ResourceId()
                : page == 7 ? pipe.GetShader(ShaderStage::Task) != ResourceId()
                            : true);
  m_PipeFlow->setStagesEnabled(enabled);
  if(tile)
    m_PipeFlow->setSelectedStage(0);
  else if(compute)
    m_PipeFlow->setSelectedStage(m_FlowPages.indexOf(6));
  else if(m_Stages->currentIndex() == 6 || (!mesh && m_Stages->currentIndex() >= 7))
    m_PipeFlow->setSelectedStage(m_FlowPages.indexOf(4));
  const bool usedOnly = !m_ShowUnused->isChecked();
  bool reflectionAvailable = false;
  bool featureInspectionAvailable = false;
  for(int i = 0; i < 6; i++)
  {
    const ShaderStage stage = MetalUIStages[i];
    auto &w = m_Shaders[i];
    const bool bound = tile ? i == 5 : i == 5 ? false : compute ? i == 2 : i != 2;
    SetShader(w, stage, bound);
    if(!bound)
      continue;
    const auto reflection = pipe.GetShaderReflection(stage);
    const auto &shader = stage == ShaderStage::Vertex ? state->vertexShader :
                         stage == ShaderStage::Fragment ? state->fragmentShader :
                         stage == ShaderStage::Compute ? state->computeShader :
                         stage == ShaderStage::Task ? state->taskShader : state->meshShader;
    const bool hasFeatures = !shader.framebufferFetch.empty() || !shader.rasterOrderGroups.empty() ||
                             shader.usesImageblock;
    featureInspectionAvailable |= (stage == ShaderStage::Fragment || i == 5) &&
                                  shader.resourceId != ResourceId();
    w.features[0]->parentWidget()->setVisible(
        (stage == ShaderStage::Fragment || i == 5) && shader.resourceId != ResourceId() &&
        (hasFeatures || m_ShowUnused->isChecked()));
    QStringList fetch, groups;
    for(auto slot : shader.framebufferFetch) fetch << tr("color[%1]").arg(slot);
    for(auto group : shader.rasterOrderGroups) groups << QString::number(group);
    const QString unknown = tr("Unavailable");
    w.features[0]->setText(fetch.isEmpty() ? (shader.metadataSource.empty() ? unknown : tr("None")) : fetch.join(lit(", ")));
    w.features[1]->setText(groups.isEmpty() ? (shader.metadataSource.empty() ? unknown : tr("None")) : groups.join(lit(", ")));
    w.features[2]->setText(shader.metadataSource.empty() ? unknown : shader.usesImageblock ? tr("Yes") : tr("No"));
    w.features[3]->setText(shader.metadataSource.empty() ? unknown : QString(shader.metadataSource));
    reflectionAvailable |= !shader.framebufferFetch.empty();
    reflectionAvailable |=
        reflection && (!reflection->constantBlocks.empty() || !reflection->readOnlyResources.empty() ||
                       !reflection->readWriteResources.empty() || !reflection->samplers.empty());
    auto bindingName = [reflection](const UsedDescriptor &b) {
      if(b.access.byteOffset >= 0x40000 && b.access.byteOffset < 0x40008)
        return tr("Input Attachment: color[%1]").arg(b.access.byteOffset - 0x40000);
      QString name = b.access.index == DescriptorAccess::NoShaderBinding && b.access.byteSize == 24
                         ? lit("Heap[%1]").arg(b.access.arrayElement)
                         : Formatter::Format(MetalDescriptorSlot(b.access));
      if(reflection && b.access.index != DescriptorAccess::NoShaderBinding)
      {
        rdcstr shaderName;
        if(b.access.type == DescriptorType::ConstantBuffer &&
           b.access.index < reflection->constantBlocks.size())
          shaderName = reflection->constantBlocks[b.access.index].name;
        else if(b.access.type == DescriptorType::Sampler &&
                b.access.index < reflection->samplers.size())
          shaderName = reflection->samplers[b.access.index].name;
        else if((b.access.type == DescriptorType::ReadWriteImage ||
                 b.access.type == DescriptorType::ReadWriteBuffer) &&
                b.access.index < reflection->readWriteResources.size())
          shaderName = reflection->readWriteResources[b.access.index].name;
        else if(b.access.index < reflection->readOnlyResources.size())
          shaderName = reflection->readOnlyResources[b.access.index].name;
        if(!shaderName.empty())
          name += lit(": ") + QString(shaderName);
      }
      return name;
    };
    for(const auto &b : pipe.GetReadOnlyResources(stage, usedOnly))
      if(b.descriptor.resource != ResourceId())
        AddDescriptor(w.resources, bindingName(b), b.descriptor)->setItalic(b.access.staticallyUnused);
    for(const auto &b : pipe.GetReadWriteResources(stage, usedOnly))
      if(b.descriptor.resource != ResourceId())
        AddDescriptor(w.uavs, bindingName(b), b.descriptor)->setItalic(b.access.staticallyUnused);
    for(const auto &b : pipe.GetSamplers(stage, usedOnly))
    {
      const auto &s = b.sampler;
      if(s.object == ResourceId())
        continue;
      const QString address =
          tr("U: %1, V: %2, W: %3").arg(ToQStr(s.addressU), ToQStr(s.addressV), ToQStr(s.addressW));
      QString filter =
          tr("Min: %1, Mag: %2, Mip: %3")
              .arg(ToQStr(s.filter.minify), ToQStr(s.filter.magnify), ToQStr(s.filter.mip));
      if(s.maxAnisotropy > 1)
        filter += tr(", Aniso: %1").arg(s.maxAnisotropy);
      if(s.filter.filter == FilterFunction::Comparison)
        filter += tr(", Compare: %1").arg(ToQStr(s.compareFunction));
      auto item =
          AddResourceRow(w.samplers,
                         {bindingName(b), address, filter,
                          tr("%1 - %2").arg(s.minLOD).arg(s.maxLOD), Formatter::Format(s.mipBias)},
                         s.object);
      item->setToolTip(m_Ctx.GetResourceName(s.object));
      item->setItalic(b.access.staticallyUnused);
    }
    for(const auto &b : pipe.GetConstantBlocks(stage, usedOnly))
    {
      const auto &d = b.descriptor;
      uint32_t needed = 0, variables = 0;
      if(reflection && b.access.index < reflection->constantBlocks.size())
      {
        const auto &cb = reflection->constantBlocks[b.access.index];
        needed = cb.byteSize;
        variables = cb.variables.size();
      }
      auto item = AddResourceRow(
          w.constants,
          {bindingName(b), d.resource == ResourceId() ? tr("Inline data") : ToQStr(d.resource),
           tr("%1 - %2").arg(d.byteOffset).arg(d.byteOffset + d.byteSize),
           tr("%1 Variables, %2 bytes needed, %3 provided").arg(variables).arg(needed).arg(d.byteSize),
           QString()},
          d.resource);
      item->setItalic(b.access.staticallyUnused);
      item->setData(0, MetalBufferOffsetRole, qulonglong(d.byteOffset));
      item->setData(0, MetalBufferSizeRole, qulonglong(d.byteSize));
      if(reflection && b.access.index < reflection->constantBlocks.size())
      {
        item->setData(0, MetalConstantStageRole, uint32_t(stage));
        item->setData(0, MetalConstantIndexRole, b.access.index);
        item->setData(0, MetalConstantArrayRole, b.access.arrayElement);
      }
    }
    const auto &buffers = stage == ShaderStage::Vertex     ? state->vertexStorageBuffers
                          : stage == ShaderStage::Fragment ? state->fragmentBuffers
                          : stage == ShaderStage::Compute  ? state->computeBuffers
                          : stage == ShaderStage::Task     ? state->taskBuffers
                                                           : state->meshBuffers;
    const auto &textures = stage == ShaderStage::Vertex     ? state->vertexTextures
                           : stage == ShaderStage::Fragment ? state->fragmentTextures
                           : stage == ShaderStage::Compute  ? state->computeTextures
                           : stage == ShaderStage::Task     ? state->taskTextures
                                                            : state->meshTextures;
    const auto &samplers = stage == ShaderStage::Vertex     ? state->vertexSamplers
                           : stage == ShaderStage::Fragment ? state->fragmentSamplers
                           : stage == ShaderStage::Compute  ? state->computeSamplers
                           : stage == ShaderStage::Task     ? state->taskSamplers
                                                            : state->meshSamplers;
    for(size_t slot = 0; slot < buffers.size(); slot++)
    {
      const auto &b = buffers[slot];
      if(b.resourceId == ResourceId() && b.byteSize)
      {
        bool exists = false;
        for(int row = 0; row < w.constants->topLevelItemCount(); row++)
          exists |= w.constants->topLevelItem(row)->text(0).split(lit(":"))[0] ==
                    Formatter::Format(uint32_t(slot));
        if(!exists)
          AddResourceRow(
              w.constants,
              {Formatter::Format(uint32_t(slot)), tr("Inline data (setBytes)"),
               tr("0 - %1").arg(b.byteSize), tr("%1 bytes provided").arg(b.byteSize), QString()},
              ResourceId());
      }
      else if(m_ShowEmpty->isChecked() && b.resourceId == ResourceId())
        AddEmptyRow(w.constants, {Formatter::Format(uint32_t(slot)), tr("Empty"), QString(),
                                  QString(), QString()});
    }
    if(m_ShowEmpty->isChecked())
    {
      for(size_t slot = 0; slot < textures.size(); slot++)
        if(textures[slot] == ResourceId())
        {
          Descriptor d;
          AddDescriptor(w.resources, Formatter::Format(uint32_t(slot)), d);
        }
      for(size_t slot = 0; slot < samplers.size(); slot++)
        if(samplers[slot] == ResourceId())
          AddEmptyRow(w.samplers, {Formatter::Format(uint32_t(slot)), tr("Empty"), QString(),
                                   QString(), QString()});
    }
  }
  m_ShowUnused->setEnabled(reflectionAvailable || featureInspectionAvailable);
  m_ShowUnused->setToolTip(
      reflectionAvailable ? tr("Show bound resources which are statically unused by the shader.")
      : featureInspectionAvailable ? tr("Show inactive or unavailable Metal shader feature metadata.")
                                   : tr("Binding reflection is not available for this pipeline."));
  auto bufferRow = [this](RDTreeWidget *tree, const QStringList &columns,
                          const MetalPipe::BufferBinding &b) {
    auto item = AddResourceRow(tree, columns, b.resourceId);
    item->setData(0, MetalBufferOffsetRole, qulonglong(b.byteOffset));
    item->setData(0, MetalBufferSizeRole, qulonglong(b.byteSize));
    return item;
  };
  if(state->indirectBuffer.resourceId != ResourceId())
  {
    RDTreeWidget *tree = compute ? m_ComputeIndirect : m_IndirectBuffer;
    const auto &b = state->indirectBuffer;
    bufferRow(tree,
              {ToQStr(b.resourceId), Formatter::Format(b.byteOffset), Formatter::Format(b.byteSize),
               compute ? tr("Dispatch Threadgroups") : tr("Draw Arguments"), QString()},
              b);
    tree->parentWidget()->show();
  }
  auto attachmentRows = [this, state](RDTreeWidget *tree) {
    for(size_t slot = 0; slot < state->colorTargets.size(); slot++)
    {
      auto id = state->colorTargets[slot].resource;
      if(id == ResourceId() && !m_ShowEmpty->isChecked()) continue;
      auto item = AddResourceRow(tree,
          {tr("Color %1").arg(slot), id == ResourceId() ? tr("Empty") : ToQStr(id),
           QString(state->attachmentStorage[slot]), QString(state->attachmentLoad[slot]),
           QString(state->attachmentStore[slot]), QString(state->attachmentStoreOptions[slot]), QString()}, id);
      if(state->attachmentStorage[slot].contains("Memoryless"))
        item->setToolTip(tr("Captured storage is Memoryless. Replay uses private backing for inspection; contents after a discard are undefined."));
    }
    if(state->depthTarget.resource != ResourceId())
      AddResourceRow(tree, {tr("Depth / Stencil"), ToQStr(state->depthTarget.resource),
          QString(state->depthStorage.empty() ? state->stencilStorage : state->depthStorage),
          QString(state->depthStorage.empty() ? state->stencilLoad : state->depthLoad),
          QString(state->depthStorage.empty() ? state->stencilStore : state->depthStore), QString(), QString()}, state->depthTarget.resource);
  };
  if(tile)
  {
    m_Tile[0]->setText(tr("%1 × %2").arg(state->tileWidth).arg(state->tileHeight));
    m_Tile[1]->setText(tr("%1 × %2 × %3").arg(state->tileThreads[0]).arg(state->tileThreads[1]).arg(state->tileThreads[2]));
    m_Tile[2]->setText(state->tileMaxThreads ? Formatter::Format(state->tileMaxThreads) : tr("Automatic"));
    m_Tile[3]->setText(state->tileSizeMatches ? tr("Yes") : tr("No"));
    m_Tile[4]->setText(tr("%1 bytes / sample").arg(state->imageblockSampleLength));
    m_Tile[5]->setText(tr("%1 bytes").arg(state->threadgroupMemoryLength));
    m_Tile[6]->setText(Formatter::Format(state->sampleCount));
    for(size_t slot = 0; slot < state->tileMemoryLengths.size(); slot++)
      if(state->tileMemoryLengths[slot] || m_ShowEmpty->isChecked())
        AddEmptyRow(m_TileMemory, {Formatter::Format(uint32_t(slot)), Formatter::Format(state->tileMemoryOffsets[slot]), Formatter::Format(state->tileMemoryLengths[slot])});
    attachmentRows(m_TileTargets);
  }
  if(compute)
    return;
  m_Pass[0]->setText(tr("%1 bytes / sample").arg(state->imageblockSampleLength));
  m_Pass[1]->setText(tr("%1 bytes").arg(state->threadgroupMemoryLength));
  attachmentRows(m_AttachmentActions);
  m_Topology->setText(ToQStr(state->topology));
  static_cast<PipelineStateViewer *>(parentWidget())
      ->setTopologyDiagram(m_TopologyDiagram, state->topology);
  for(const auto &a : state->vertexAttributes)
  {
    const bool layout = a.bufferIndex < state->vertexBuffers.size();
    const auto b = layout ? state->vertexBuffers[a.bufferIndex] : MetalPipe::VertexBuffer();
    AddResourceRow(
        m_VertexAttributes,
        {Formatter::Format(a.attributeIndex), lit("ATTRIBUTE"), Formatter::Format(a.attributeIndex),
         QString(a.format.Name()), Formatter::Format(a.bufferIndex), Formatter::Format(a.byteOffset),
         layout ? QString(b.stepFunction) : QString(), Formatter::Format(b.stepRate), QString()},
        ResourceId());
  }
  auto vertexRow = [this](const QString &slot, const MetalPipe::VertexBuffer &b, int index) {
    auto item =
        AddResourceRow(m_VertexBuffers,
                       {slot, b.resourceId == ResourceId() ? tr("Empty") : ToQStr(b.resourceId),
                        Formatter::Format(b.byteStride), Formatter::Format(b.byteOffset),
                        Formatter::Format(b.byteSize), QString()},
                       b.resourceId);
    item->setData(0, MetalBufferOffsetRole, qulonglong(b.byteOffset));
    item->setData(0, MetalBufferSizeRole, qulonglong(b.byteSize));
    item->setData(0, MetalBufferSlotRole, index);
    item->setData(0, MetalBufferSizeRole + 10, b.byteStride);
  };
  if(state->indexBuffer.resourceId != ResourceId() || m_ShowEmpty->isChecked())
    vertexRow(tr("Index"), state->indexBuffer, -1);
  for(size_t slot = 0; slot < state->vertexBuffers.size(); slot++)
  {
    bool input = false;
    for(const auto &attribute : state->vertexAttributes)
      input |= attribute.bufferIndex == slot;
    // Metal's vertex buffer namespace also contains shader/argument buffers. Only vertex
    // descriptor layouts belong to IA; shader bindings are displayed on the VS page.
    if(!input && (!m_ShowUnused->isChecked() || state->vertexBuffers[slot].stepFunction.empty()))
      continue;
    if(state->vertexBuffers[slot].resourceId != ResourceId() || m_ShowEmpty->isChecked())
      vertexRow(Formatter::Format(uint32_t(slot)), state->vertexBuffers[slot], int(slot));
  }
  m_VertexAttributes->setToolTip(
      state->vertexAttributes.empty()
          ? tr("The Metal vertex descriptor has no fixed-function attributes. Inspect Vertex "
               "Shader resources for shader-fetched inputs.")
          : QString());
  const auto &r = state->rasterizer;
  auto flag = [this](QLabel *label, bool value) {
    label->setPixmap(value ? Pixmaps::tick(this) : Pixmaps::cross(this));
    label->setToolTip(value ? tr("Enabled") : tr("Disabled"));
  };
  m_Raster[0]->setText(ToQStr(r.fillMode));
  m_Raster[1]->setText(ToQStr(r.cullMode));
  flag(m_Raster[2], r.frontCCW);
  m_Raster[3]->setText(Formatter::Format(r.depthBias));
  m_Raster[4]->setText(Formatter::Format(r.depthBiasClamp));
  m_Raster[5]->setText(Formatter::Format(r.slopeScaledDepthBias));
  flag(m_Raster[6], r.depthClip);
  flag(m_Raster[7], r.rasterizationEnabled);
  m_Raster[8]->setText(Formatter::Format(state->sampleCount));
  const bool vrr = r.rasterizationRateMap != ResourceId();
  m_VRR[0]->setText(vrr ? tr("Yes") : tr("No"));
  m_VRR[1]->setText(vrr && r.rateMapScreenSize.size() == 2 ?
                    tr("%1 × %2").arg(r.rateMapScreenSize[0]).arg(r.rateMapScreenSize[1]) : lit("—"));
  m_VRR[2]->setText(vrr ? QString::number(r.rateMapHorizontal.size()) : lit("0"));
  m_VRRMap->parentWidget()->setVisible(vrr || m_ShowEmpty->isChecked());
  m_VRRLayers->parentWidget()->setVisible(vrr || m_ShowEmpty->isChecked());
  if(vrr)
  {
    AddResourceRow(m_VRRMap, {ToQStr(r.rasterizationRateMap), lit("Go")}, r.rasterizationRateMap);
    auto rates = [](const rdcarray<float> &values) {
      QStringList strings;
      for(float value : values) strings << Formatter::Format(value);
      return strings.join(lit(", "));
    };
    for(size_t i = 0; i < r.rateMapHorizontal.size(); i++)
    {
      QString size = r.rateMapPhysicalSizes.size() > i*2+1 ?
          tr("%1 × %2").arg(r.rateMapPhysicalSizes[i*2]).arg(r.rateMapPhysicalSizes[i*2+1]) : tr("Unavailable");
      AddEmptyRow(m_VRRLayers, {QString::number(i), size, rates(r.rateMapHorizontal[i]),
                              i < r.rateMapVertical.size() ? rates(r.rateMapVertical[i]) : tr("Unavailable")});
    }
  }
  for(size_t slot = 0; slot < r.viewports.size(); slot++)
  {
    const auto &v = r.viewports[slot];
    if(v.enabled)
      AddResourceRow(m_Viewports,
                     {Formatter::Format(uint32_t(slot)), Formatter::Format(v.x),
                      Formatter::Format(v.y), Formatter::Format(v.width), Formatter::Format(v.height),
                      Formatter::Format(v.minDepth), Formatter::Format(v.maxDepth)},
                     ResourceId());
  }
  for(size_t slot = 0; slot < r.scissors.size(); slot++)
  {
    const auto &s = r.scissors[slot];
    if(s.enabled)
      AddResourceRow(
          m_Scissors,
          {Formatter::Format(uint32_t(slot)), Formatter::Format(s.x), Formatter::Format(s.y),
           Formatter::Format(s.width), Formatter::Format(s.height)},
          ResourceId());
  }
  for(size_t slot = 0; slot < state->colorTargets.size(); slot++)
    if(state->colorTargets[slot].resource != ResourceId() || m_ShowEmpty->isChecked())
      AddDescriptor(m_Targets, Formatter::Format(uint32_t(slot)), state->colorTargets[slot]);
  if(state->depthTarget.resource != ResourceId() || m_ShowEmpty->isChecked())
    AddDescriptor(m_Targets, tr("Depth"), state->depthTarget);
  for(size_t slot = 0; slot < state->resolveTargets.size(); slot++)
    if(state->resolveTargets[slot].resource != ResourceId())
      AddDescriptor(m_ResolveTargets, Formatter::Format(uint32_t(slot)), state->resolveTargets[slot]);
  m_ResolveTargets->parentWidget()->setVisible(m_ResolveTargets->topLevelItemCount() > 0);
  for(size_t slot = 0; slot < state->colorBlends.size(); slot++)
  {
    if(!m_ShowEmpty->isChecked() &&
       (slot >= state->colorTargets.size() || state->colorTargets[slot].resource == ResourceId()))
      continue;
    const auto &b = state->colorBlends[slot];
    const QString mask = QFormatStr("%1%2%3%4")
                             .arg(b.writeMask & 1 ? lit("R") : lit("_"))
                             .arg(b.writeMask & 2 ? lit("G") : lit("_"))
                             .arg(b.writeMask & 4 ? lit("B") : lit("_"))
                             .arg(b.writeMask & 8 ? lit("A") : lit("_"));
    AddResourceRow(m_ColorBlends,
                   {Formatter::Format(uint32_t(slot)), b.enabled ? tr("True") : tr("False"),
                    ToQStr(b.colorBlend.source), ToQStr(b.colorBlend.destination),
                    ToQStr(b.colorBlend.operation), ToQStr(b.alphaBlend.source),
                    ToQStr(b.alphaBlend.destination), ToQStr(b.alphaBlend.operation), mask},
                   ResourceId());
  }
  flag(m_Blend[0], state->alphaToCoverageEnabled);
  flag(m_Blend[1], state->alphaToOneEnabled);
  m_Blend[2]->setText(tr("%1, %2, %3, %4")
                          .arg(state->blendFactor[0])
                          .arg(state->blendFactor[1])
                          .arg(state->blendFactor[2])
                          .arg(state->blendFactor[3]));
  const auto &d = state->depthStencil;
  flag(m_Depth[0], state->depthTarget.resource != ResourceId() &&
                       (d.depthWrites || d.depthFunction != CompareFunction::AlwaysTrue));
  m_Depth[1]->setText(ToQStr(d.depthFunction));
  m_Depth[2]->setText(d.depthWrites ? tr("Enabled") : tr("Read-Only"));
  if(d.stencilEnabled || m_ShowEmpty->isChecked())
    for(int face = 0; face < 2; face++)
    {
      const auto &f = face ? d.backFace : d.frontFace;
      auto item = AddResourceRow(
          m_Stencil,
          {face ? tr("Back") : tr("Front"), ToQStr(f.function), ToQStr(f.failOperation),
           ToQStr(f.depthFailOperation), ToQStr(f.passOperation), Formatter::Format(f.writeMask, true),
           Formatter::Format(f.compareMask, true), Formatter::Format(f.reference, true)},
          ResourceId());
      auto common = static_cast<PipelineStateViewer *>(parentWidget());
      common->SetStencilTreeItemValue(item, 5, uint8_t(f.writeMask));
      common->SetStencilTreeItemValue(item, 6, uint8_t(f.compareMask));
      common->SetStencilTreeItemValue(item, 7, uint8_t(f.reference));
    }
  if(tess)
  {
    m_Tessellation[0]->setText(Formatter::Format(state->patchControlPoints));
    m_Tessellation[1]->setText(QString(state->tessellationPartitionMode));
    m_Tessellation[2]->setText(Formatter::Format(state->maxTessellationFactor));
    m_Tessellation[3]->setText(lit("Half"));
    m_Tessellation[4]->setText(QString(state->tessellationStepFunction));
    m_Tessellation[5]->setText(QString(state->tessellationOutputWinding));
    flag(m_Tessellation[6], state->tessellationFactorScaleEnabled);
    m_Tessellation[7]->setText(Formatter::Format(state->tessellationInstanceStride));
    m_Tessellation[8]->setText(Formatter::Format(state->tessellationFactorScale));
    const auto &b = state->tessellationFactors;
    if(b.resourceId != ResourceId())
      bufferRow(m_TessellationBuffer,
                {ToQStr(b.resourceId), Formatter::Format(b.byteOffset),
                 Formatter::Format(b.byteSize), QString()},
                b);
  }
}

void MetalPipelineStateViewer::ExportHTMLTree(QXmlStreamWriter &xml, const QString &title,
                                              RDTreeWidget *tree)
{
  xml.writeStartElement(lit("h2"));
  xml.writeCharacters(title);
  xml.writeEndElement();
  QStringList headers = tree->getHeaders();
  if(headers.last() == tr("Go"))
    headers.removeLast();
  QList<QVariantList> rows;
  for(int row = 0; row < tree->topLevelItemCount(); row++)
  {
    QVariantList values;
    for(int column = 0; column < headers.size(); column++)
      values << tree->topLevelItem(row)->text(column);
    rows << values;
  }
  static_cast<PipelineStateViewer *>(parentWidget())->exportHTMLTable(xml, headers, rows);
}
void MetalPipelineStateViewer::ExportHTML()
{
  auto common = static_cast<PipelineStateViewer *>(parentWidget());
  auto xml = common->beginHTMLExport();
  if(!xml)
    return;
  for(int stage = 0; stage < 6; stage++)
  {
    xml->writeStartElement(lit("h1"));
    xml->writeCharacters(stage == 5 ? tr("Tile Shader") : ToQStr(MetalUIStages[stage], GraphicsAPI::Metal));
    xml->writeEndElement();
    const auto &w = m_Shaders[stage];
    common->exportHTMLTable(*xml, {tr("Pipeline"), tr("Shader"), tr("Entry Point")},
                            {w.pipeline->text(), w.resource->text(), w.entryPoint->toolTip()});
    ExportHTMLTree(*xml, tr("Resources"), w.resources);
    ExportHTMLTree(*xml, tr("UAVs"), w.uavs);
    ExportHTMLTree(*xml, tr("Samplers"), w.samplers);
    ExportHTMLTree(*xml, tr("Constant Buffers"), w.constants);
    if(!w.features[0]->parentWidget()->isHidden())
    {
      QStringList featureNames;
      QVariantList featureValues;
      for(auto label : w.features) { featureNames << label->accessibleName(); featureValues << label->text(); }
      common->exportHTMLTable(*xml, featureNames, featureValues);
    }
  }
  for(auto tree :
      {m_VertexAttributes, m_VertexBuffers, m_IndirectBuffer, m_TessellationBuffer, m_Viewports,
       m_Scissors, m_Targets, m_ResolveTargets, m_ColorBlends, m_Stencil, m_ComputeIndirect, m_TileMemory, m_TileTargets, m_AttachmentActions, m_FXResources, m_VRRMap, m_VRRLayers})
    ExportHTMLTree(*xml, tree->accessibleName(), tree);
  for(const auto &labels : {m_Raster, m_Blend, m_Depth, m_Tessellation, m_Tile, m_Pass, m_FX, m_FXTemporal, m_VRR})
  {
    QStringList headers;
    QVariantList values;
    for(auto label : labels)
    {
      headers << label->accessibleName();
      values << (label->text().isEmpty() ? label->toolTip() : label->text());
    }
    common->exportHTMLTable(*xml, headers, values);
  }
  common->endHTMLExport(xml);
}
void MetalPipelineStateViewer::OnCaptureLoaded()
{
  SetState();
}
void MetalPipelineStateViewer::OnCaptureClosed()
{
  ClearState();
  SetFlow(false, false);
  m_PipeFlow->setStagesEnabled({false, false, false, false, false, false, false});
}
void MetalPipelineStateViewer::OnEventChanged(uint32_t)
{
  SetState();
}
void MetalPipelineStateViewer::SelectPipelineStage(PipelineStage stage)
{
  int page = -1;
  switch(stage)
  {
    case PipelineStage::VertexInput: page = 0; break;
    case PipelineStage::VertexShader: page = 1; break;
    case PipelineStage::Rasterizer: page = 3; break;
    case PipelineStage::PixelShader: page = 4; break;
    case PipelineStage::ColorDepthOutput:
    case PipelineStage::SampleMask: page = 5; break;
    case PipelineStage::ComputeShader: page = m_FlowPages.contains(9) ? 9 : 6; break;
    default: break;
  }
  const int index = m_FlowPages.indexOf(page);
  if(index >= 0)
    m_PipeFlow->setSelectedStage(index);
}
