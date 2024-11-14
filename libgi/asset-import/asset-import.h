#pragma once
#include "libgi/scene.h"

#include <filesystem>
#include <string>
#include <optional>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>

namespace import {
	class asset_importer {
		protected:
		const std::string &name;
		const glm::mat4 &trafo;
		const uint subdiv_level;
		std::filesystem::path filepath;

		public:
		asset_importer(const std::string &name, const glm::mat4 &trafo, const uint subdiv_level = 0)
			: name(name), trafo(trafo), subdiv_level(subdiv_level) {}
		virtual ~asset_importer() {}

		virtual void load_scene(const std::filesystem::path& path) = 0;
		virtual void import(scene& scene) = 0;
	};

	class assimp_importer : public asset_importer {
		private:
		Assimp::Importer importer;
		//const aiScene* scene_ai;
		std::optional<const aiScene *> opt_scene_ai;

		public:
		assimp_importer(const std::string &name, const glm::mat4 &trafo, const uint subdiv_level = 0)
			: asset_importer(name, trafo, subdiv_level) {}
		~assimp_importer() {}

		void load_scene(const std::filesystem::path& path) override;
		void import(scene& scene) override;
	};

	class usd_importer : public asset_importer {
		private:
		//UsdStageRefPtr stage;

		public:
		usd_importer(const std::string &name, const glm::mat4 &trafo, const uint subdiv_level = 0)
			: asset_importer(name, trafo, subdiv_level) {}
		//usd_importer() : asset_importer(name, glm::mat4(0)) {}
		~usd_importer() {}

		void load_scene(const std::filesystem::path& path) override;
		void import(scene& scene) override;
	};

	class objx_importer : public asset_importer {
		public:
		objx_importer(const std::string &name, const glm::mat4 &trafo, const uint subdiv_level = 0)
			: asset_importer(name, trafo, subdiv_level) {}
		~objx_importer() {}

		void load_scene(const std::filesystem::path& path) override;
		void import(scene& scene) override;
	};
}