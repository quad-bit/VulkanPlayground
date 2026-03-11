#include "ImguiEditor.h"
#include "plog/Log.h"
#include <string>
#include "Components.h"

Common::ImguiEditor::ImguiEditor(const ImguiUtil& utilObj, const SceneManager& sceneManager) : cm_utilObj(utilObj), cm_sceneManager(sceneManager)
{
    m_selectedNodeIndex = cm_sceneManager.GetParentList()[0].id();

    auto CreateSceneHierarchyPanel = [this]() -> void
    {
        bool oneTimeCheck = false;
        auto CreateSceneTrees = [this, &oneTimeCheck](auto self, const flecs::entity& e) -> void
        {
            ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            // open by default for the root node only
            if (!oneTimeCheck)
            {
                base_flags = base_flags | ImGuiTreeNodeFlags_DefaultOpen;
                oneTimeCheck = true;
            }

            {
                if (e.id() == m_selectedNodeIndex)
                    base_flags = base_flags | ImGuiTreeNodeFlags_Selected;
            }

            if (!Common::HasChildren(e))
                base_flags = base_flags | ImGuiTreeNodeFlags_Leaf;

            if (ImGui::TreeNodeEx((std::to_string(e.id())).c_str(), base_flags, "%s", e.name().c_str()))
            {
                if (ImGui::IsItemClicked())
                {
                    PLOGD << e.name();
                    m_selectedNodeIndex = e.id();
                }

                // Iterate children recursively
                e.children([&](const flecs::entity& child)
                {
                    self(self, child);
                });

                ImGui::TreePop();
            }
        };

        ImGui::Begin("Scene");
        for(auto& parent : cm_sceneManager.GetParentList())
            CreateSceneTrees(CreateSceneTrees, parent);
        ImGui::End();
    };

    auto CreateTransformPanel = [this]() -> void
    {
        auto e = cm_sceneManager.m_world.entity(m_selectedNodeIndex);

        auto t = e.get<Common::Transform>();
        auto mat = t.m_modelMat;
        glm::vec3 position = t.m_position;
        glm::vec3 scale = t.m_scale;
        glm::vec3 angles = t.m_eulerAngles;

        ImGui::Begin("Transform");

        ImGui::BeginTable("TransformVec", 4, ImGuiTableFlags_Borders);
        //for (int row = 0; row < 3; row++)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            for (int col = 1; col < 4; col++)
            {
                ImGui::TableSetColumnIndex(col);
                std::string cell_id = "##pos_" + std::to_string(0) + "_" + std::to_string(col);
                ImGui::SetNextItemWidth(60);
                ImGui::InputFloat(cell_id.c_str(), &position[col-1], 0.0f, 0.0f, "%.3f");
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            for (int col = 1; col < 4; col++)
            {
                ImGui::TableSetColumnIndex(col);
                std::string cell_id = "##scale_" + std::to_string(1) + "_" + std::to_string(col);
                ImGui::SetNextItemWidth(60);
                ImGui::InputFloat(cell_id.c_str(), &scale[col-1], 0.0f, 0.0f, "%.3f");
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Angles");
            for (int col = 1; col < 4; col++)
            {
                ImGui::TableSetColumnIndex(col);
                std::string cell_id = "##ang_" + std::to_string(2) + "_" + std::to_string(col);
                ImGui::SetNextItemWidth(60);
                ImGui::InputFloat(cell_id.c_str(), &angles[col-1], 0.0f, 0.0f, "%.3f");
            }
        }
        ImGui::EndTable();

        // Create a table with 4 columns
        ImGui::Text("ModelMatrix");
        if (ImGui::BeginTable("MatrixTable", 4, ImGuiTableFlags_Borders))
        {
            for (int row = 0; row < 4; row++)
            {
                ImGui::TableNextRow();
                for (int col = 0; col < 4; col++)
                {
                    ImGui::TableSetColumnIndex(col);

                    // Create a unique ID for each cell
                    std::string cell_id = "##cell_" + std::to_string(row) + "_" + std::to_string(col);

                    // Editable float input for each matrix element
                    ImGui::SetNextItemWidth(60);
                    ImGui::InputFloat(cell_id.c_str(), &mat[row][col], 0.0f, 0.0f, "%.3f");
                }
            }
            ImGui::EndTable();
        }

        ImGui::End();
    };

    auto CreateMeshPanel = [this]()
    {
        auto e = cm_sceneManager.m_world.entity(m_selectedNodeIndex);
        if (e.has<Common::Mesh>())
        {
            ImGui::Begin("Mesh");
            auto& mesh = e.get<Common::Mesh>();
            for (auto& view : mesh.m_meshViews)
            {
                ImGui::Text("Index count : %d", view.m_indexCount);
                ImGui::Text("Triangle count : %d", view.m_indexCount/3);
            }
            ImGui::End();
        }
    };

    cm_utilObj.AddPersistentDrawCalls(CreateSceneHierarchyPanel);
    cm_utilObj.AddPersistentDrawCalls(CreateTransformPanel);
    cm_utilObj.AddPersistentDrawCalls(CreateMeshPanel);
}
