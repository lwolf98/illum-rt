#pragma once
#include "libgi/scene.h"

#include <filesystem>
#include <string>
#include <optional>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdShade/shader.h>

namespace import {
	class asset_importer {
		protected:
		const load_config &cfg;

		public:
		asset_importer(const load_config &cfg)
			: cfg(cfg) {}
		virtual ~asset_importer() {}

		virtual void load_scene() = 0;
		virtual void import(scene& scene) = 0;
	};

	class assimp_importer : public asset_importer {
		private:
		Assimp::Importer importer;
		const aiScene* scene_ai;

		public:
		assimp_importer(const load_config &cfg)
			: asset_importer(cfg) {}
		~assimp_importer() {}

		void load_scene() override;
		void import(scene& scene) override;
	};

	class usd_importer : public asset_importer {
		private:
		pxr::UsdStageRefPtr stage;
		// material_map associates an USD material path to an material id relative to an asset import.
		// The map will be reset on a call of usd_importer::import
		std::map<std::string, int> material_map;
		int load_material(const pxr::UsdPrim &prim, scene& scene);
		void traverse_shader_inputs(const pxr::UsdShadeShader &shader, int level, material &material);

		public:
		usd_importer(const load_config &cfg)
			: asset_importer(cfg) {}
		~usd_importer() {}

		void load_scene() override;
		void import(scene& scene) override;
	};

	class objx_importer : public asset_importer {
		public:
		objx_importer(const load_config &cfg)
			: asset_importer(cfg) {}
		~objx_importer() {}

		void load_scene() override;
		void import(scene& scene) override;
	};
}