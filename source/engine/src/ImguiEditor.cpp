#include "ImguiEditor.h"
#include "plog/Log.h"
#include <string>
#include "Components.h"

Loops::ImguiEditor::ImguiEditor(const ImguiUtil& utilObj, const SceneManager& sceneManager, BoundsManager& boundsManager) :
    cm_utilObj(utilObj), cm_sceneManager(sceneManager), cm_boundsManager(boundsManager)
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

            if (!HasChildren(e))
                base_flags = base_flags | ImGuiTreeNodeFlags_Leaf;

            if (ImGui::TreeNodeEx((std::to_string(e.id())).c_str(), base_flags, "%s", e.name().c_str()))
            {
                if (ImGui::IsItemClicked())
                {
                    //PLOGD << e.name();
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

        auto& t = e.get<Loops::Transform>();
        auto mat = t.m_modelMat;
        auto globalMat = t.m_modelMatGlobal;
        glm::vec3 position = t.m_position;
        glm::vec3 scale = t.m_scale;
        glm::vec3 angles = t.m_eulerAngles;

        ImGui::Begin("Transform");

        if (ImGui::BeginTable("TransformVec", 4, ImGuiTableFlags_Borders))
        {
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
                    ImGui::InputFloat(cell_id.c_str(), &position[col - 1], 0.0f, 0.0f, "%.3f");
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Scale");
                for (int col = 1; col < 4; col++)
                {
                    ImGui::TableSetColumnIndex(col);
                    std::string cell_id = "##scale_" + std::to_string(1) + "_" + std::to_string(col);
                    ImGui::SetNextItemWidth(60);
                    ImGui::InputFloat(cell_id.c_str(), &scale[col - 1], 0.0f, 0.0f, "%.3f");
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Angles");
                for (int col = 1; col < 4; col++)
                {
                    ImGui::TableSetColumnIndex(col);
                    std::string cell_id = "##ang_" + std::to_string(2) + "_" + std::to_string(col);
                    ImGui::SetNextItemWidth(60);
                    ImGui::InputFloat(cell_id.c_str(), &angles[col - 1], 0.0f, 0.0f, "%.3f");
                }
            }
            ImGui::EndTable();
        }

        // Create a table with 4 columns
        if (ImGui::BeginTabBar("Transformations"))
        {
            if (ImGui::BeginTabItem("LocalMatrix"))
            {
                if (ImGui::BeginTable("LocalMatrixTable", 4, ImGuiTableFlags_Borders))
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
                            ImGui::InputFloat(cell_id.c_str(), &mat[col][row], 0.0f, 0.0f, "%.3f");
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GlobalMatrix"))
            {
                if (ImGui::BeginTable("GLobalMatrixTable", 4, ImGuiTableFlags_Borders))
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
                            ImGui::InputFloat(cell_id.c_str(), &globalMat[col][row], 0.0f, 0.0f, "%.3f");
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    };

    auto CreateMeshPanel = [this]()
    {
        auto e = cm_sceneManager.m_world.entity(m_selectedNodeIndex);
        if (e.has<Loops::Mesh>())
        {
            ImGui::Begin("Mesh");
            auto& mesh = e.get<Loops::Mesh>();
            for (uint32_t i = 0; i<mesh.m_meshViewCount; i++)
            {
                const Loops::MeshView& view = mesh.m_meshViews[i];
                ImGui::Text("Index count : %d", view.m_indexCount);
                ImGui::Text("Triangle count : %d", view.m_indexCount/3);
            }
            ImGui::End();
        }
    };

    auto CreateBvhPanel = [this]()
        {
            const Loops::BvhCreationMethod& creationMethod = cm_boundsManager.GetCreationMethod();
            const Loops::SplitMethod& splitMethod = cm_boundsManager.GetSplitType();

            bool isRecursiveActive = creationMethod == Loops::BvhCreationMethod::RECURSIVE ? true : false;

            if (ImGui::Begin("BVH settings"))
            {
                ImVec2 currentCurPos = ImGui::GetCursorPos();

                if(ImGui::RadioButton("Recursive", isRecursiveActive) || isRecursiveActive)
                {
                    cm_boundsManager.SetCreationMethod(BvhCreationMethod::RECURSIVE);
                    ImGui::SetCursorPosX(currentCurPos.x + 10);
                    ImGui::BeginGroup();
                    //currentCurPos = ImGui::GetCursorPos();
                    if (ImGui::RadioButton("Equal count", splitMethod == SplitMethod::EQUAL_COUNT))
                    {
                        cm_boundsManager.SetSplitType(SplitMethod::EQUAL_COUNT);
                    }
                    else if (ImGui::RadioButton("Mid", splitMethod == SplitMethod::MID))
                    {
                        cm_boundsManager.SetSplitType(SplitMethod::MID);
                    }
                    else if (ImGui::RadioButton("SAH", splitMethod == SplitMethod::SAH))
                    {
                        cm_boundsManager.SetSplitType(SplitMethod::SAH);
                    }
                    ImGui::EndGroup();
                }

                auto pos = ImVec2(ImGui::GetWindowSize().x * 0.5f, currentCurPos.y);
                ImGui::SetCursorPos(pos);
                if (ImGui::RadioButton("Linear Morton", !isRecursiveActive))
                {
                    cm_boundsManager.SetCreationMethod(BvhCreationMethod::LINEAR);
                }

                ImGui::End();
            }
        };

    cm_utilObj.AddPersistentDrawCalls(CreateSceneHierarchyPanel);
    cm_utilObj.AddPersistentDrawCalls(CreateTransformPanel);
    cm_utilObj.AddPersistentDrawCalls(CreateMeshPanel);
    cm_utilObj.AddPersistentDrawCalls(CreateBvhPanel);
}

