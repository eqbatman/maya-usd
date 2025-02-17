//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef PXRUSDMAYA_TRANSFORM_WRITER_H
#define PXRUSDMAYA_TRANSFORM_WRITER_H

#include <mayaUsd/base/api.h>
#include <mayaUsd/fileio/primWriter.h>
#include <mayaUsd/fileio/writeJobContext.h>

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdUtils/sparseValueWriter.h>

#include <maya/MEulerRotation.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <unordered_map>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// Writes transforms and serves as the base class for custom transform writers.
/// Handles the conversion of Maya transformation data into USD xformOps.
class UsdMayaTransformWriter : public UsdMayaPrimWriter
{
public:
    MAYAUSD_CORE_PUBLIC
    UsdMayaTransformWriter(
        const MFnDependencyNode& depNodeFn,
        const SdfPath&           usdPath,
        UsdMayaWriteJobContext&  jobCtx);

    /// Main export function that runs when the traversal hits the node.
    /// This extends UsdMayaPrimWriter::Write() by exporting xform ops for
    /// UsdGeomXformable if the Maya node has transform data.
    MAYAUSD_CORE_PUBLIC
    void Write(const UsdTimeCode& usdTime) override;

private:
    using _TokenRotationMap
        = std::unordered_map<const TfToken, MEulerRotation, TfToken::HashFunctor>;

    enum class _XformType
    {
        Translate,
        Rotate,
        Scale,
        Shear
    };
    enum class _SampleType
    {
        None,
        Static,
        Animated
    };

    // This may not be the best name here as it isn't necessarily animated.
    struct _AnimChannel
    {
        MPlug       plug[3];
        _SampleType sampleType[3];
        // defValue should always be in "maya" space.  that is, if it's a
        // rotation it should be radians, not degrees. (This is done so we only
        // need to do conversion in one place, and so that, if we need to do
        // euler filtering, we don't do conversions, and then undo them to use
        // MEulerRotation).
        GfVec3d                   defValue;
        _XformType                opType;
        UsdGeomXformOp::Type      usdOpType;
        UsdGeomXformOp::Precision precision;
        TfToken                   opName;
        bool                      isInverse;
        UsdGeomXformOp            op;
    };

    // For a given array of _AnimChannels and time, compute the xformOp data if
    // needed and set the xformOps' values.
    static void _ComputeXformOps(
        const std::vector<_AnimChannel>&           animChanList,
        const UsdTimeCode&                         usdTime,
        const bool                                 eulerFilter,
        UsdMayaTransformWriter::_TokenRotationMap* previousRotates,
        UsdUtilsSparseValueWriter*                 valueWriter);

    // Creates an _AnimChannel from a Maya compound attribute if there is
    // meaningful data. This means we found data that is non-identity.
    // Returns true if we extracted an _AnimChannel and false otherwise (e.g.
    // the data was identity).
    static bool _GatherAnimChannel(
        const _XformType           opType,
        const MFnTransform&        iTrans,
        const TfToken&             parentName,
        const MString&             xName,
        const MString&             yName,
        const MString&             zName,
        std::vector<_AnimChannel>* oAnimChanList,
        const bool                 isWritingAnimation,
        const bool                 setOpName);

    // Change the channel suffix so that the USD XformOp becomes unique.
    // This is to deal with complex rigs that can have multiple transforms
    // affecting the same transform operation on the same UsdGeomXformable.
    void _MakeAnimChannelsUnique(const UsdGeomXformable& usdXformable);

    /// Populates the AnimChannel vector with various ops based on
    /// the Maya transformation logic. If scale and/or rotate pivot are
    /// declared, creates inverse ops in the appropriate order.
    void _PushTransformStack(
        const MFnTransform&     iTrans,
        const UsdGeomXformable& usdXForm,
        const bool              writeAnim);

    std::vector<_AnimChannel> _animChannels;
    _TokenRotationMap         _previousRotates;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
