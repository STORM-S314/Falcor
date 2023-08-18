/***************************************************************************
#	Copyright (c) 2018, Christoph Schied
#	All rights reserved.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions are met:
#		* Redistributions of source code must retain the above copyright
#		  notice, this list of conditions and the following disclaimer.
#		* Redistributions in binary form must reproduce the above copyright
#		  notice, this list of conditions and the following disclaimer in the
#		  documentation and/or other materials provided with the distribution.
#		* Neither the name of the Karlsruhe Institute of Technology nor the
#		  names of its contributors may be used to endorse or promote products
#		  derived from this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
#	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
import Scene.Scene;
import Scene.HitInfo;

uint getMeshInstanceID(HitInfo hit)
{
        uint instanceID = 10000000;
        uint materialID = 10000000;
        uint geometryID = 10000000;

        switch (hit.getType())
        {
        case HitType::Triangle:
        {
            const TriangleHit triangleHit = hit.getTriangleHit();
            instanceID = triangleHit.instanceID.index;
            geometryID = gScene.getGeometryInstance(triangleHit.instanceID).geometryID;
            materialID = gScene.getMaterialID(triangleHit.instanceID);
            break;
        }
        case HitType::DisplacedTriangle:
        {
            const DisplacedTriangleHit displacedTriangleHit = hit.getDisplacedTriangleHit();
            instanceID = displacedTriangleHit.instanceID.index;
            geometryID = gScene.getGeometryInstance(displacedTriangleHit.instanceID).geometryID;
            materialID = gScene.getMaterialID(displacedTriangleHit.instanceID);
            break;
        }
        case HitType::Curve:
        {
            const CurveHit curveHit = hit.getCurveHit();
            instanceID = curveHit.instanceID.index;
            geometryID = gScene.getGeometryInstance(curveHit.instanceID).geometryID;
            materialID = gScene.getMaterialID(curveHit.instanceID);
            break;
        }
        case HitType::SDFGrid:
        {
            const SDFGridHit sdfGridHit = hit.getSDFGridHit();
            instanceID = sdfGridHit.instanceID.index;
            geometryID = gScene.getGeometryInstance(sdfGridHit.instanceID).geometryID;
            materialID = gScene.getMaterialID(sdfGridHit.instanceID);
            break;
        }
    }
    return instanceID;
}
