#include "asset-import.h"

#include <iostream>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/input.h>

using namespace import;
using namespace pxr;

void TraverseShaderInputs(const UsdShadeShader& shader, int level) {
	std::string space(level, '\t');

	TfToken shaderId;
	shader.GetIdAttr().Get(&shaderId);
	std::cout << space << "Shader id: " << shaderId << std::endl;
	/*if (shaderId == "UsdUVTexture") {
		std::cout << "TESTESTESTESTESTESTESTESTEST" << std::endl;
		VtValue inputValue;
		shader.GetInput(TfToken("file")).Get(&inputValue);
		std::cout << inputValue << std::endl;
		std::cout << "TESTESTESTESTESTESTESTESTEST" << std::endl;
	}*/

	// Iterate over shader inputs
	std::vector<UsdShadeInput> shaderInputs = shader.GetInputs();
	for (const auto& shaderInput : shaderInputs) {
		std::cout << space << "Shader input: " << shaderInput.GetFullName() << std::endl;

		// Check if the input is connected to another shader
		UsdShadeConnectableAPI source;
		TfToken inputName;
		UsdShadeAttributeType type;
		if (shaderInput.GetConnectedSource(&source, &inputName, &type)) {
			UsdPrim sourcePrim = source.GetPrim();
			if (sourcePrim.IsA<UsdShadeShader>()) {
				// If the input is connected to another shader, print the connected shader path
				UsdShadeShader connectedShader(sourcePrim);
				std::cout << space << "Input connected to shader: " << connectedShader.GetPath() << std::endl;
				
				// You can now recursively handle the connected shader's inputs as well
				TraverseShaderInputs(connectedShader, level+1);
			}
			else {
				// If the input is not connected to another shader, fetch its value (color, float, etc.)
				VtValue inputValue;
				if (shaderInput.Get(&inputValue)) {
					std::cout << space << "Input value: " << inputValue << std::endl;
				}
			}
		} else {
			// If not connected, print the input value
			VtValue inputValue;
			if (shaderInput.Get(&inputValue)) {
				std::cout << space << "Input value: " << inputValue << std::endl;
			}
		}
	}
}

void usd_importer::load_scene(const std::filesystem::path& filepath) {
	//UsdStageRefPtr *stage;
	//stage = UsdStage::Open(filepath);

	std::cout << "USD importer load called!" << std::endl;

	// Open the stage (USD file)
	UsdStageRefPtr stage = UsdStage::Open(filepath.c_str());
	if (!stage) {
		std::cerr << "Failed to open USD file: " << filepath.c_str() << std::endl;
		return;
	}

	for (auto prim : stage->Traverse()) {
		if (prim.IsA<UsdGeomMesh>()) {
			UsdGeomMesh mesh(prim);
			std::cout << "Found mesh: " << prim.GetPath() << std::endl;

			// Process the mesh data (see next step)
			// Accessing points (vertices)
			VtArray<GfVec3f> points;
			mesh.GetPointsAttr().Get(&points);
			std::cout << "Number of vertices: " << points.size() << std::endl;

			// Accessing face vertex indices
			VtArray<int> faceVertexIndices;
			mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
			std::cout << "Number of face vertex indices: " << faceVertexIndices.size() << std::endl;

			// Accessing face vertex counts
			VtArray<int> faceVertexCounts;
			mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
			std::cout << "Number of faces: " << faceVertexCounts.size() << std::endl;

			VtArray<GfVec3f> normals;
			if (mesh.GetNormalsAttr().Get(&normals)) {
				std::cout << "Number of normals: " << normals.size() << std::endl;
			}
			else {
				std::cout << "No normals" << std::endl;
			}

			UsdGeomPrimvarsAPI primvarsAPI(mesh);
			UsdGeomPrimvar stPrimvar = primvarsAPI.GetPrimvar(TfToken("st"));
			VtArray<GfVec2f> uvs;
			if (stPrimvar && stPrimvar.Get(&uvs)) {
				std::cout << "Number of UV coordinates: " << uvs.size() << std::endl;
			}
			else {
				std::cout << "No UVs" << std::endl;
			}

			UsdShadeMaterialBindingAPI binding(mesh);
			//UsdShadeMaterial material = binding.GetMaterial();
			UsdRelationship materialRel = binding.GetDirectBindingRel();
			if (materialRel) {
				std::vector<SdfPath> materialPaths;
				materialRel.GetTargets(&materialPaths);

				if (!materialPaths.empty()) {
					std::cout << "Number of assigned materials: " << materialPaths.size() << std::endl;

					// Now we have the material, let's load it
					UsdShadeMaterial material(stage->GetPrimAtPath(materialPaths[0]));  // Resolve the material
					std::cout << "Material assigned to mesh: " << material.GetPath() << std::endl;

					// Accessing the material's surface output (it connects to the shader)
					UsdShadeOutput surfaceOutput = material.GetOutput(TfToken("surface"));
					if (surfaceOutput) {
						std::cout << "Material has a surface output connected to: " << surfaceOutput.GetFullName() << std::endl;

						// Now, traverse the connected shader inputs
						//UsdShadeShader shader = surfaceOutput.GetConnectedSource().GetPrim().GetChild<UsdShadeShader>();
						UsdShadeConnectableAPI source;
						TfToken inputName;
						UsdShadeAttributeType type;
						if (!surfaceOutput.GetConnectedSource(&source, &inputName, &type)) {
							std::cout << "No connected source found for surface output." << std::endl;
							return;
						}
						//UsdShadeShader shader = source.GetPrim().GetChild<UsdShadeShader>();
						UsdShadeShader shader(source.GetPrim());
						if (shader) {
							std::cout << "Shader connected to surface output: " << shader.GetPath() << std::endl;

							TraverseShaderInputs(shader, 1);
						}
					} else {
						std::cout << "Material surface output is not connected." << std::endl;
					}
				}
			}
			else {
				std::cout << "Could not resolve material :(" << std::endl;
			}
		}
		else {
			std::cout << "No mesh: " << prim.GetPath() << std::endl;
		}
	}
}

void usd_importer::import(scene& scene) {

}