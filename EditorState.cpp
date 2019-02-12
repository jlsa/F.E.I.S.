//
// Created by Symeon on 23/12/2018.
//

#include <cmath>
#include <filesystem>
#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include "EditorState.h"
#include "tinyfiledialogs.h"
#include "Toolbox.h"

EditorState::EditorState(Fumen &fumen) : fumen(fumen) {
    reloadFromFumen();
}

void EditorState::reloadFromFumen() {
    if (not this->fumen.Charts.empty()) {
        this->selectedChart = this->fumen.Charts.begin()->second;
    } else {
        this->selectedChart.reset();
    }
    reloadMusic();
    reloadJacket();
}

/*
 * Reloads music from what's indicated in the "music path" field of the fumen
 * Resets the music state in case anything fails
 * Updates playbackPosition and the chartRuntime
 */
void EditorState::reloadMusic() {
    music.emplace();
    if (!music->openFromFile(
            (fumen.path.parent_path() / std::filesystem::path(fumen.musicPath)).string())
            ) {
        music.reset();
    }
    reloadPlaybackPositionAndChartRuntime();
}

/*
 * NEVER CALL THAT YOURSELF,
 * Let reloadMusic do it,
 * you can end up with some strange stuff if you call it before reloadMusic
 */
void EditorState::reloadPlaybackPositionAndChartRuntime() {
    playbackPosition = sf::seconds(-(fumen.offset));
    previousPos = playbackPosition;
    if (music) {
        if (selectedChart) {
            chartRuntime = sf::seconds(std::max(music->getDuration().asSeconds(),fumen.getChartRuntime(*selectedChart)-fumen.offset)+2.f);
        } else {
            chartRuntime = sf::seconds(std::max(-fumen.offset,music->getDuration().asSeconds()));
        }
    } else {
        if (selectedChart) {
            chartRuntime = sf::seconds(std::max(fumen.getChartRuntime(*selectedChart)-fumen.offset,2.f));
        } else {
            chartRuntime = sf::seconds(std::max(-fumen.offset,2.f));
        }
    }
}

/*
 * Reloads the jacket from what's indicated in the "jacket path" field of the fumen
 * Resets the jacket state if anything fails
 */
void EditorState::reloadJacket() {
    jacket.emplace();
    if (!jacket->loadFromFile((fumen.path.parent_path() / std::filesystem::path(fumen.jacketPath)).string())) {
        jacket.reset();
    }
}

void EditorState::updateMusicVolume() {
    if (music) {
        Toolbox::updateVolume(*music,musicVolume);
    }
}

void EditorState::setPlaybackAndMusicPosition(sf::Time newPosition) {

    if (newPosition.asSeconds() < -fumen.offset) {
        newPosition = sf::seconds(-fumen.offset);
    } else if (newPosition > chartRuntime) {
        newPosition = chartRuntime;
    }
    playbackPosition = newPosition;
    if (music) {
        if (playbackPosition.asSeconds() >= 0 and playbackPosition < music->getDuration()) {
            music->setPlayingOffset(playbackPosition);
        }
    }
}

void EditorState::displayPlayfield(Marker& marker, MarkerEndingState markerEndingState) {

    ImGui::SetNextWindowSize(ImVec2(400,400),ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0,0),ImVec2(FLT_MAX,FLT_MAX),Toolbox::CustomConstraints::ContentSquare);

    if (ImGui::Begin("Playfield",&showPlayfield,ImGuiWindowFlags_NoScrollbar)) {

        float squareSize = ImGui::GetWindowSize().x / 4.f;
        float TitlebarHeight = ImGui::GetWindowSize().y - ImGui::GetWindowSize().x;

        if (selectedChart) {

            int ImGuiIndex = 0;

            for (auto note : visibleNotes) {

                float note_offset = (playbackPosition.asSeconds() - getSecondsAt(note.getTiming()));
                auto frame = static_cast<long long int>(std::floor(note_offset * 30.f));
                int x = note.getPos()%4;
                int y = note.getPos()/4;

                if (note.getLength() == 0) {

                    auto t = marker.getSprite(markerEndingState,note_offset);

                    if (t) {
                        ImGui::SetCursorPos({x*squareSize,TitlebarHeight + y*squareSize});
                        ImGui::PushID(ImGuiIndex);
                        ImGui::Image(*t,{squareSize,squareSize});
                        ImGui::PopID();
                        ++ImGuiIndex;
                    }

                } else {

                    float tail_end_in_seconds = getSecondsAt(note.getTiming()+note.getLength());
                    float tail_end_offset = playbackPosition.asSeconds() - tail_end_in_seconds;

                    if (playbackPosition.asSeconds() < tail_end_in_seconds) {

                        int triangle = note.getTail_pos_as_note_pos();

                        auto triangle_x = static_cast<float>(triangle%4);
                        auto triangle_y = static_cast<float>(triangle/4);

                        AffineTransform<float> x_trans(0.0f,ticksToSeconds(note.getLength()),triangle_x, static_cast<float>(x));
                        AffineTransform<float> y_trans(0.0f,ticksToSeconds(note.getLength()),triangle_y, static_cast<float>(y));
                        triangle_x = x_trans.clampedTransform(note_offset);
                        triangle_y = y_trans.clampedTransform(note_offset);

                        auto tail_tex = playfield.longNoteMarker.getTailTexture(note_offset,note.getTail_pos());
                        if (tail_tex) {

                            ImVec2 cursorPos;
                            sf::Vector2f texSize;

                            if (frame < 8) {

                                // Before the note : tail goes from triangle tip to note edge

                                switch (note.getTail_pos()%4) {

                                    // going down
                                    case 0:
                                        cursorPos.x = x*squareSize;
                                        cursorPos.y = (triangle_y+1)*squareSize;
                                        texSize.x = squareSize;
                                        texSize.y = (y - triangle_y - 1)*squareSize;
                                        break;

                                    // going left (to the left, to the left ...)
                                    case 1:
                                        cursorPos.x = (x+1)*squareSize;
                                        cursorPos.y = y*squareSize;
                                        texSize.x = (triangle_x - x - 1)*squareSize;
                                        texSize.y = squareSize;
                                        break;

                                    // going up
                                    case 2:
                                        cursorPos.x = x*squareSize;
                                        cursorPos.y = (y+1)*squareSize;
                                        texSize.x = squareSize;
                                        texSize.y = (triangle_y - y - 1)*squareSize;
                                        break;

                                    // going right
                                    case 3:
                                        cursorPos.x = (triangle_x+1)*squareSize;
                                        cursorPos.y = y*squareSize;
                                        texSize.x = (x - triangle_x - 1)*squareSize;
                                        texSize.y = squareSize;
                                        break;

                                    default:
                                        throw std::runtime_error("wtf ?");
                                }

                            } else {

                                // During the note : tail goes from triangle base to note edge

                                switch (note.getTail_pos()%4) {

                                    // going down
                                    case 0:
                                        cursorPos.x = x*squareSize;
                                        cursorPos.y = (triangle_y + 0.9f)*squareSize;
                                        texSize.x = squareSize;
                                        texSize.y = (y - triangle_y - 0.9f)*squareSize;
                                        break;

                                    // going left (to the left, to the left ...)
                                    case 1:
                                        cursorPos.x = (x+1)*squareSize;
                                        cursorPos.y = y*squareSize;
                                        texSize.x = (triangle_x - x - 0.9f)*squareSize;
                                        texSize.y = squareSize;
                                        break;

                                    // going up
                                    case 2:
                                        cursorPos.x = x*squareSize;
                                        cursorPos.y = (y+1)*squareSize;
                                        texSize.x = squareSize;
                                        texSize.y = (triangle_y - y - 0.9f)*squareSize;
                                        break;

                                    // going right
                                    case 3:
                                        cursorPos.x = (triangle_x + 0.9f)*squareSize;
                                        cursorPos.y = y*squareSize;
                                        texSize.x = (x - triangle_x - 0.9f)*squareSize;
                                        texSize.y = squareSize;
                                        break;

                                    default:
                                        throw std::runtime_error("wtf ?");
                                }

                            }

                            cursorPos.y += TitlebarHeight;

                            ImGui::SetCursorPos(cursorPos);
                            ImGui::PushID(ImGuiIndex);
                            ImGui::Image(*tail_tex,texSize);
                            ImGui::PopID();
                            ++ImGuiIndex;

                            Toolbox::displayIfHasValue(
                                    playfield.longNoteMarker.getSquareBackgroundTexture(note_offset, note.getTail_pos()),
                                    {x*squareSize,TitlebarHeight + y*squareSize},
                                    {squareSize,squareSize},
                                    ImGuiIndex
                                    );

                            Toolbox::displayIfHasValue(
                                    playfield.longNoteMarker.getSquareOutlineTexture(note_offset, note.getTail_pos()),
                                    {x*squareSize,TitlebarHeight + y*squareSize},
                                    {squareSize,squareSize},
                                    ImGuiIndex
                            );

                            Toolbox::displayIfHasValue(
                                    playfield.longNoteMarker.getTriangleTexture(note_offset, note.getTail_pos()),
                                    {triangle_x*squareSize,TitlebarHeight + triangle_y*squareSize},
                                    {squareSize,squareSize},
                                    ImGuiIndex
                            );

                            Toolbox::displayIfHasValue(
                                    playfield.longNoteMarker.getSquareHighlightTexture(note_offset, note.getTail_pos()),
                                    {x*squareSize,TitlebarHeight + y*squareSize},
                                    {squareSize,squareSize},
                                    ImGuiIndex
                            );

                            // Display the beginning marker
                            auto t = marker.getSprite(markerEndingState,note_offset);
                            if (t) {
                                ImGui::SetCursorPos({x*squareSize,TitlebarHeight + y*squareSize});
                                ImGui::PushID(ImGuiIndex);
                                ImGui::Image(*t,{squareSize,squareSize});
                                ImGui::PopID();
                                ++ImGuiIndex;
                            }
                        }

                    } else {

                        // Display the ending marker
                        if (tail_end_offset > 0.0f) {
                            auto t = marker.getSprite(markerEndingState,tail_end_offset);
                            if (t) {
                                ImGui::SetCursorPos({x*squareSize,TitlebarHeight + y*squareSize});
                                ImGui::PushID(ImGuiIndex);
                                ImGui::Image(*t,{squareSize,squareSize});
                                ImGui::PopID();
                                ++ImGuiIndex;
                            }
                        }
                    }
                }
            }
        }

        // display buttons over
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                ImGui::PushID(x+4*y);
                ImGui::SetCursorPos({x*squareSize,TitlebarHeight + y*squareSize});
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0,0,1.f,0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0,0,1.f,0.5f));
                if (ImGui::ImageButton(playfield.button,{squareSize,squareSize},0)) {
                    toggleNoteAtCurrentTime(x+4*y);
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }
        }
    }
    ImGui::End();

}

/*
 * Display all metadata in an editable form
 */
void EditorState::displayProperties() {
    ImGui::SetNextWindowSize(ImVec2(500,240));
    ImGui::Begin("Properties",&showProperties,ImGuiWindowFlags_NoResize);
    {
        ImGui::Columns(2, nullptr, false);

        if (jacket) {
            ImGui::Image(*jacket,sf::Vector2f(200,200));
        } else {
            ImGui::BeginChild("Jacket",ImVec2(200,200),true);
            ImGui::EndChild();
        }


        ImGui::NextColumn();
        ImGui::InputText("Title",&fumen.songTitle);
        ImGui::InputText("Artist",&fumen.artist);
        if (Toolbox::InputTextColored(music.has_value(),"Invalid Music Path","Music",&fumen.musicPath)) {
            reloadMusic();
        }
        if (Toolbox::InputTextColored(jacket.has_value(),"Invalid Jacket Path","Jacket",&(fumen.jacketPath))) {
            reloadJacket();
        }
        if(ImGui::InputFloat("BPM",&fumen.BPM,1.0f,10.0f)) {
            if (fumen.BPM <= 0.0f) {
                fumen.BPM = 0.0f;
            }
        }
        ImGui::InputFloat("offset",&fumen.offset,0.01f,1.f);
    }
    ImGui::End();
}

/*
 * Display any information that would be useful for the user to troubleshoot the status of the editor
 * will appear in the "Editor Status" window
 */
void EditorState::displayStatus() {
    ImGui::Begin("Status",&showStatus,ImGuiWindowFlags_AlwaysAutoResize);
    {
        if (not music) {
            if (not fumen.musicPath.empty()) {
                ImGui::TextColored(ImVec4(1,0.42,0.41,1),"Invalid music path : %s",fumen.musicPath.c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0.42,0.41,1),"No music file loaded");
            }
        }

        if (not jacket) {
            if (not fumen.jacketPath.empty()) {
                ImGui::TextColored(ImVec4(1,0.42,0.41,1),"Invalid jacket path : %s",fumen.jacketPath.c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0.42,0.41,1),"No jacket loaded");
            }
        }
        if (ImGui::SliderInt("Music Volume",&musicVolume,0,10)) {
            updateMusicVolume();
        }
    }
    ImGui::End();
}

void EditorState::displayPlaybackStatus() {

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 25), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin(
            "Playback Status",
            &showPlaybackStatus,
            ImGuiWindowFlags_NoNav
            |ImGuiWindowFlags_NoDecoration
            |ImGuiWindowFlags_NoInputs
            |ImGuiWindowFlags_NoMove
            |ImGuiWindowFlags_AlwaysAutoResize
            );
    {
        if (selectedChart) {
            ImGui::Text("%s %d",selectedChart->get().dif_name.c_str(),selectedChart->get().level); ImGui::SameLine();
        } else {
            ImGui::TextDisabled("No chart selected"); ImGui::SameLine();
        }
        ImGui::TextDisabled("Snap : "); ImGui::SameLine();
        ImGui::Text("%s",Toolbox::toOrdinal(snap*4).c_str()); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.53,0.53,0.53,1),"Beats :"); ImGui::SameLine();
        ImGui::Text("%02.2f",this->getBeats()); ImGui::SameLine();
        if (music) {
            ImGui::TextColored(ImVec4(0.53,0.53,0.53,1),"Music File Offset :"); ImGui::SameLine();
            ImGui::TextUnformatted(Toolbox::to_string(music->getPlayingOffset()).c_str()); ImGui::SameLine();
        }
        ImGui::TextColored(ImVec4(0.53,0.53,0.53,1),"Timeline Position :"); ImGui::SameLine();
        ImGui::TextUnformatted(Toolbox::to_string(playbackPosition).c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorState::displayTimeline() {

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 25, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::SetNextWindowSize({20,io.DisplaySize.y * 0.9f},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::Begin(
            "Timeline",
            &showTimeline,
            ImGuiWindowFlags_NoNav
            |ImGuiWindowFlags_NoDecoration
            |ImGuiWindowFlags_NoTitleBar
            |ImGuiWindowFlags_NoMove
            );
    {
        if (music) {
            AffineTransform<float> scroll(-fumen.offset,chartRuntime.asSeconds(),1.f,0.f);
            float slider_pos = scroll.transform(playbackPosition.asSeconds());
            ImGui::SetCursorPos({0,0});
            if(ImGui::VSliderFloat("",ImGui::GetContentRegionMax(),&slider_pos,0.f,1.f,"")) {
                setPlaybackAndMusicPosition(sf::seconds(scroll.backwards_transform(slider_pos)));
                this->lastTimingTicked = -1;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void EditorState::displayChartList() {

    if (ImGui::Begin("Chart List",&showChartList,ImGuiWindowFlags_AlwaysAutoResize)) {
        if (this->fumen.Charts.empty()) {
            ImGui::Dummy({100,0}); ImGui::SameLine();
            ImGui::Text("- no charts -"); ImGui::SameLine();
            ImGui::Dummy({100,0});
        } else {
            ImGui::Dummy(ImVec2(300,0));
            ImGui::Columns(3, "mycolumns");
            ImGui::TextDisabled("Difficulty"); ImGui::NextColumn();
            ImGui::TextDisabled("Level"); ImGui::NextColumn();
            ImGui::TextDisabled("Note Count"); ImGui::NextColumn();
            ImGui::Separator();
            for (auto& tuple : fumen.Charts) {
                if (ImGui::Selectable(tuple.first.c_str(), selectedChart ? selectedChart->get()==tuple.second : false , ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedChart = tuple.second;
                }
                ImGui::NextColumn();
                ImGui::Text("%d",tuple.second.level); ImGui::NextColumn();
                ImGui::Text("%d", static_cast<int>(tuple.second.Notes.size())); ImGui::NextColumn();
                ImGui::PushID(&tuple);
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

/*
 * This SCREAAAAMS for optimisation, but in the meantime it works !
 */
void EditorState::updateVisibleNotes() {

    visibleNotes.clear();

    if (selectedChart) {

        float position = playbackPosition.asSeconds();

        for (auto const& note : selectedChart->get().Notes) {

            float note_timing_in_seconds = getSecondsAt(note.getTiming());

            // we can leave early if the note is happening too far after the position
            if (position > note_timing_in_seconds - 16.f/30.f) {
                if (note.getLength() == 0) {
                    if (position < note_timing_in_seconds + 16.f/30.f) {
                        visibleNotes.insert(note);
                    }
                } else {
                    float tail_end_in_seconds = getSecondsAt(note.getTiming()+note.getLength());
                    if (position < tail_end_in_seconds + 16.f/30.f) {
                        visibleNotes.insert(note);
                    }
                }
            }
        }
    }
}

/*
 * If a note is visible for the given pos, delete it
 * Otherwise create note at nearest tick
 */
void EditorState::toggleNoteAtCurrentTime(int pos) {
    if (selectedChart) {
        bool deleted_something = false;
        for (auto note : visibleNotes) {
            if (note.getPos() == pos) {
                selectedChart->get().Notes.erase(note);
                deleted_something = true;
            }
        }
        if (not deleted_something) {
            selectedChart->get().Notes.emplace(pos,static_cast<int>(roundf(getTicks())));
        }
    }
}

void ESHelper::save(EditorState& ed) {
    try {
        ed.fumen.autoSaveAsMemon();
    } catch (const std::exception& e) {
        tinyfd_messageBox("Error",e.what(),"ok","error",1);
    }
}

void ESHelper::open(std::optional<EditorState> &ed) {
    const char* _filepath = tinyfd_openFileDialog("Open File",nullptr,0,nullptr,nullptr,false);
    if (_filepath != nullptr) {
        ESHelper::openFromFile(ed,_filepath);
    }
}

void ESHelper::openFromFile(std::optional<EditorState> &ed, std::filesystem::path path) {
    try {
        Fumen f(path);
        f.autoLoadFromMemon();
        ed.emplace(f);
        Toolbox::pushNewRecentFile(std::filesystem::canonical(ed->fumen.path));
    } catch (const std::exception &e) {
        tinyfd_messageBox("Error", e.what(), "ok", "error", 1);
    }
}

/*
 * Returns the newly created chart if there is
 */
std::optional<Chart> ESHelper::NewChartDialog::display(EditorState &editorState) {

    std::optional<Chart> newChart;
    if (ImGui::Begin(
            "New Chart",
            &editorState.showNewChartDialog,
            ImGuiWindowFlags_NoResize
            |ImGuiWindowFlags_AlwaysAutoResize))
    {

        if (showCustomDifName) {
            comboPreview = "Custom";
        } else {
            if (difficulty.empty()) {
                comboPreview = "Choose One";
            } else {
                comboPreview = difficulty;
            }
        }
        if(ImGui::BeginCombo("Difficulty",comboPreview.c_str())) {
            for (auto dif_name : {"BSC","ADV","EXT"}) {
                if (editorState.fumen.Charts.find(dif_name) == editorState.fumen.Charts.end()) {
                    if(ImGui::Selectable(dif_name,dif_name == difficulty)) {
                        showCustomDifName = false;
                        difficulty = dif_name;
                    }
                } else {
                    ImGui::TextDisabled(dif_name);
                }
            }
            ImGui::Separator();
            if (ImGui::Selectable("Custom",&showCustomDifName)) {
                difficulty = "";
            }
            ImGui::EndCombo();
        }
        if (showCustomDifName) {
            Toolbox::InputTextColored(
                    editorState.fumen.Charts.find(difficulty) == editorState.fumen.Charts.end(),
                    "Chart name has to be unique",
                    "Difficulty Name",
                    &difficulty
                    );
        }
        ImGui::InputInt("Level",&level);
        ImGui::Separator();
        if (ImGui::TreeNode("Advanced##New Chart")) {
            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
            if (ImGui::InputInt("Resolution",&resolution)) {
                if (resolution < 1) {
                    resolution = 1;
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Number of ticks in a beat");
                ImGui::BulletText("Has nothing to do with time signature");
                ImGui::BulletText("Leave the default unless you know what you're doing");
                ImGui::EndTooltip();
            }
            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            ImGui::TreePop();
        }
        ImGui::Separator();
        if (difficulty.empty() or (editorState.fumen.Charts.find(difficulty) != editorState.fumen.Charts.end())) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Button("Create Chart##New Chart");
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        } else {
            if (ImGui::Button("Create Chart##New Chart")) {
                try {
                    newChart.emplace(difficulty,level,resolution);
                } catch (const std::exception& e) {
                    tinyfd_messageBox("Error",e.what(),"ok","error",1);
                }
            }
        }
    }
    ImGui::End();
    return newChart;
}

void ESHelper::ChartPropertiesDialog::display(EditorState &editorState) {

    assert(editorState.selectedChart.has_value());

    if (this->shouldRefreshValues) {

        shouldRefreshValues = false;

        difNamesInUse.clear();
        this->level = editorState.selectedChart->get().level;
        this->difficulty = editorState.selectedChart->get().dif_name;
        std::set<std::string> difNames{"BSC","ADV","EXT"};
        showCustomDifName = (difNames.find(difficulty) == difNames.end());

        for (auto const& tuple : editorState.fumen.Charts) {
            if (tuple.second != editorState.selectedChart) {
                difNamesInUse.insert(tuple.first);
            }
        }
    }

    if (ImGui::Begin(
            "Chart Properties",
            &editorState.showChartProperties,
            ImGuiWindowFlags_NoResize
            |ImGuiWindowFlags_AlwaysAutoResize))
    {

        if (showCustomDifName) {
            comboPreview = "Custom";
        } else {
            if (difficulty.empty()) {
                comboPreview = "Choose One";
            } else {
                comboPreview = difficulty;
            }
        }
        if(ImGui::BeginCombo("Difficulty",comboPreview.c_str())) {
            for (auto dif_name : {"BSC","ADV","EXT"}) {
                if (difNamesInUse.find(dif_name) == difNamesInUse.end()) {
                    if(ImGui::Selectable(dif_name,dif_name == difficulty)) {
                        showCustomDifName = false;
                        difficulty = dif_name;
                    }
                } else {
                    ImGui::TextDisabled(dif_name);
                }
            }
            ImGui::Separator();
            if (ImGui::Selectable("Custom",&showCustomDifName)) {
                difficulty = "";
            }
            ImGui::EndCombo();
        }
        if (showCustomDifName) {
            Toolbox::InputTextColored(
                    difNamesInUse.find(difficulty) == difNamesInUse.end(),
                    "Chart name has to be unique",
                    "Difficulty Name",
                    &difficulty
            );
        }
        ImGui::InputInt("Level",&level);
        ImGui::Separator();
        if (difficulty.empty() or (difNamesInUse.find(difficulty) != difNamesInUse.end())) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Button("Apply##New Chart");
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        } else {
            if (ImGui::Button("Apply##New Chart")) {
                try {
                    editorState.fumen.Charts.erase(editorState.selectedChart->get().dif_name);
                    editorState.selectedChart->get().dif_name = this->difficulty;
                    editorState.selectedChart->get().level = this->level;
                    if (not (editorState.fumen.Charts.emplace(this->difficulty,editorState.selectedChart.value())).second) {
                        throw std::runtime_error("Could not insert modified chart in fumen");
                    } else {
                        shouldRefreshValues = true;
                    }
                } catch (const std::exception& e) {
                    tinyfd_messageBox("Error",e.what(),"ok","error",1);
                }
            }
        }
    }
    ImGui::End();

}
