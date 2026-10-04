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

#pragma once

#include <QFrame>
#include "Code/Interface/QRDInterface.h"

class QLabel;
class RDLabel;
class QTabWidget;
class QToolButton;
class QVBoxLayout;
class QXmlStreamWriter;
class PipelineFlowChart;
class RDTreeWidget;
class RDTreeWidgetItem;

class MetalPipelineStateViewer : public QFrame, public ICaptureViewer
{
public:
  explicit MetalPipelineStateViewer(ICaptureContext &ctx, QWidget *parent = 0);

  void OnCaptureLoaded() override;
  void OnCaptureClosed() override;
  void OnSelectedEventChanged(uint32_t eventId) override {}
  void OnEventChanged(uint32_t eventId) override;

  void SelectPipelineStage(PipelineStage stage);
  ResourceId GetResource(RDTreeWidgetItem *item);

private:
  struct ShaderWidgets
  {
    RDLabel *pipeline = NULL;
    RDLabel *resource = NULL;
    QLabel *entryPoint = NULL;
    QList<QLabel *> features;
    QToolButton *view = NULL;
    QToolButton *edit = NULL;
    QToolButton *save = NULL;
    RDTreeWidget *resources = NULL;
    RDTreeWidget *uavs = NULL;
    RDTreeWidget *samplers = NULL;
    RDTreeWidget *constants = NULL;
  };
  ShaderWidgets MakeShader(QVBoxLayout *layout, ShaderStage stage);
  void SetShader(ShaderWidgets &widgets, ShaderStage stage, bool bound);
  void ViewShader(ShaderStage stage);
  QVBoxLayout *MakeStagePage(const QString &title);
  RDTreeWidget *MakeTree(QVBoxLayout *layout, const QString &title, const QStringList &headers);
  QList<QLabel *> MakeSummary(QVBoxLayout *layout, const QString &title, const QStringList &labels,
                              int columns);
  RDTreeWidgetItem *AddDescriptor(RDTreeWidget *tree, const QString &binding,
                                  const Descriptor &descriptor);
  RDTreeWidgetItem *AddResourceRow(RDTreeWidget *tree, const QStringList &values, ResourceId id);
  RDTreeWidgetItem *AddEmptyRow(RDTreeWidget *tree, const QStringList &values);
  void ExportHTMLTree(QXmlStreamWriter &xml, const QString &title, RDTreeWidget *tree);
  void ExportHTML();
  void SetState();
  void ClearState();
  void SetFlow(bool mesh, bool tessellation, bool tile = false, bool metalFX = false);

  ICaptureContext &m_Ctx;
  PipelineFlowChart *m_PipeFlow = NULL;
  QTabWidget *m_Stages = NULL;
  QToolButton *m_ShowUnused = NULL, *m_ShowEmpty = NULL, *m_Export = NULL;
  QList<int> m_FlowPages;
  ShaderWidgets m_Shaders[6];
  QLabel *m_Topology = NULL, *m_TopologyDiagram = NULL;
  RDLabel *m_MeshView = NULL;
  QList<QLabel *> m_FXTemporal, m_VRR, m_FX, m_Tile, m_Pass, m_Raster, m_Blend, m_Depth, m_Tessellation;
  RDTreeWidget *m_VertexAttributes = NULL, *m_VertexBuffers = NULL, *m_IndirectBuffer = NULL;
  RDTreeWidget *m_FXResources = NULL, *m_VRRLayers = NULL, *m_VRRMap = NULL;
  RDTreeWidget *m_TileMemory = NULL, *m_TileTargets = NULL, *m_AttachmentActions = NULL;
  RDTreeWidget *m_Viewports = NULL, *m_Scissors = NULL;
  RDTreeWidget *m_Targets = NULL, *m_ResolveTargets = NULL, *m_ColorBlends = NULL;
  RDTreeWidget *m_Stencil = NULL, *m_TessellationBuffer = NULL, *m_ComputeIndirect = NULL;
};
