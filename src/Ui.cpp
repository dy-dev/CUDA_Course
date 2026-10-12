#include "Ui.h"

#include "Params.h"
#include "Timing.h"

#include <cmath>

// raygui est une bibliothèque en un seul en-tête : son implémentation est
// compilée ici et nulle part ailleurs. Ses avertissements sont neutralisés
// localement.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 4267 4305 4996)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wenum-compare"
#endif
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace
{
    constexpr float kMargin = 12.0f;
    constexpr float kRowHeight = 24.0f;
    constexpr float kLabelHeight = 18.0f;
    constexpr float kRowGap = 4.0f;
    constexpr int kTextSize = 16;

    Color textColor()
    {
        return GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL));
    }

    // Empile les contrôles de haut en bas dans la largeur du panneau.
    struct Column
    {
        float y = kMargin;

        Rectangle row(float height = kRowHeight)
        {
            Rectangle bounds = { kMargin, y, kPanelWidth - 2.0f * kMargin, height };
            y += height + kRowGap;
            return bounds;
        }

        void section(const char* title)
        {
            y += kRowGap;
            GuiLine(row(16.0f), title);
        }

        void label(const char* text)
        {
            GuiLabel(row(kLabelHeight), text);
        }

        // Deux libellés côte à côte, chacun dans une demi-largeur.
        void pair(const char* left, const char* right)
        {
            Rectangle bounds = row(kLabelHeight);
            bounds.width /= 2.0f;
            GuiLabel(bounds, left);
            bounds.x += bounds.width;
            GuiLabel(bounds, right);
        }

        // Case à cocher carrée, libellé à droite.
        void checkBox(const char* text, bool* checked)
        {
            Rectangle box = row(kLabelHeight);
            box.width = box.height;
            GuiCheckBox(box, text, checked);
        }

        // Deux cases à cocher sur une même ligne.
        void checkBoxPair(const char* left, bool* leftChecked, const char* right, bool* rightChecked)
        {
            Rectangle box = row(kLabelHeight);
            const float half = box.width / 2.0f;
            box.width = box.height;
            GuiCheckBox(box, left, leftChecked);
            box.x += half;
            GuiCheckBox(box, right, rightChecked);
        }

        // Groupe de boutons exclusifs répartis sur la largeur ("A;B;C").
        void toggleGroup(const char* items, int count, int* active)
        {
            Rectangle bounds = row();
            const float padding = float(GuiGetStyle(TOGGLE, GROUP_PADDING));
            bounds.width = (bounds.width - padding * float(count - 1)) / float(count);
            GuiToggleGroup(bounds, items, active);
        }
    };

    int roundToThousand(float value)
    {
        return int(std::lround(value / 1000.0f)) * 1000;
    }

    void drawCountControls(Column& col, UiState& ui)
    {
        col.section("Particules (touches 1 à 5)");

        // Curseur : la valeur n'est appliquée qu'au relâchement du bouton.
        float slider = float(ui.pendingCount);
        GuiSlider(col.row(), nullptr, nullptr, &slider, float(kMinCount), float(kMaxCount));
        if (roundToThousand(slider) != roundToThousand(float(ui.pendingCount)))
        {
            ui.pendingCount = roundToThousand(slider);
        }

        // Champ numérique : la valeur est appliquée en fin de saisie.
        if (GuiValueBox(col.row(), nullptr, &ui.pendingCount, kMinCount, kMaxCount, ui.countEditing))
        {
            ui.countEditing = !ui.countEditing;
        }

        if (!ui.countEditing && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ui.pendingCount != ui.count)
        {
            ui.count = ui.pendingCount;
        }

        if (GuiButton(col.row(), "Réinitialiser"))
        {
            ui.resetRequested = true;
        }
    }

    void drawEngineControls(Column& col, UiState& ui)
    {
        col.section(ui.gpuAvailable ? "Moteur (touche G)" : "Moteur : GPU absent");

        int selected = ui.useGpu ? 1 : 0;
        // Sans moteur GPU, la bascule reste affichée mais inactive.
        if (!ui.gpuAvailable)
        {
            GuiDisable();
        }
        col.toggleGroup("CPU;GPU", 2, &selected);
        GuiEnable();
        ui.useGpu = selected == 1;
    }

    void drawPhysicsControls(Column& col, UiState& ui, const UiStats& stats)
    {
        col.section("Physique");
        col.checkBoxPair("Murs", &ui.walls, "Collisions", &ui.collisions);
        // Restitution : libellé dans la moitié gauche, curseur dans la moitié droite.
        Rectangle restitution = col.row(kLabelHeight);
        restitution.width /= 2.0f;
        GuiLabel(restitution, TextFormat("Restit. %.2f", ui.restitution));
        restitution.x += restitution.width;
        GuiSlider(restitution, nullptr, nullptr, &ui.restitution, kMinRestitution, 1.0f);
        if (stats.cpuCollisionsSkipped)
        {
            col.label(TextFormat("CPU : aucune au-delà de %dk", kCpuCollisionLimit / 1000));
        }
        col.label("Collisions sur la carte :");
        col.toggleGroup("Directes;Grille", 2, &ui.collisionMode);
        col.checkBox("Colorer par cellule (C)", &ui.colorByCell);
    }

    void drawMouseControls(Column& col, UiState& ui)
    {
        col.section("Souris : clic, appui long");
        col.label(TextFormat("Explosion : %.0f px/s", ui.explosion));
        GuiSlider(col.row(16.0f), nullptr, nullptr, &ui.explosion, 0.0f, kMaxExplosion);
        col.label(TextFormat("Attraction : %.0f px/s²", ui.attraction));
        GuiSlider(col.row(16.0f), nullptr, nullptr, &ui.attraction, 0.0f, kMaxAttraction);
    }

    const char* formatMs(float ms)
    {
        return ms < 0.0f ? "-" : TextFormat("%.3f ms", ms);
    }

    // Facteur de vitesse entre le moteur actif et la dernière mesure de l'autre.
    const char* formatFactor(const UiStats& stats)
    {
        if (stats.stepMs <= 0.0f || stats.otherStepMs <= 0.0f)
        {
            return "-";
        }
        if (stats.stepMs <= stats.otherStepMs)
        {
            return TextFormat("%s %.1fx plus rapide", stats.engine, speedup(stats.otherStepMs, stats.stepMs));
        }
        return TextFormat("%s %.1fx plus rapide", stats.otherEngine, speedup(stats.stepMs, stats.otherStepMs));
    }

    void drawStats(Column& col, const UiStats& stats)
    {
        col.section("Mesures");
        col.label(TextFormat("FPS %d    N %d", stats.fps, stats.count));
        col.pair(TextFormat("%s %s", stats.engine, formatMs(stats.stepMs)),
                 TextFormat("%s %s", stats.otherEngine, formatMs(stats.otherStepMs)));
        col.label(formatFactor(stats));
        if (stats.directCollideMs > 0.0f && stats.gridCollideMs > 0.0f)
        {
            col.label(TextFormat("Grille : collisions %.1fx", speedup(stats.directCollideMs, stats.gridCollideMs)));
        }
        else
        {
            col.label("Grille : -");
        }
    }

    // Durée des dernières images, avec la limite de 16,7 ms (60 images/s).
    void drawFrameGraph(Rectangle area, const FrameHistory& history)
    {
        constexpr float kMaxMs = 50.0f;
        const Color line = GetColor(GuiGetStyle(DEFAULT, LINE_COLOR));

        DrawRectangleLinesEx(area, 1.0f, line);
        const float budgetY = area.y + area.height * (1.0f - (1000.0f / 60.0f) / kMaxMs);
        DrawLineEx({ area.x, budgetY }, { area.x + area.width, budgetY }, 1.0f, ORANGE);

        const float step = area.width / float(FrameHistory::kCapacity - 1);
        auto pointAt = [&](int age)
        {
            const float ms = history.at(age).frameMs < kMaxMs ? history.at(age).frameMs : kMaxMs;
            return Vector2{ area.x + step * float(age), area.y + area.height * (1.0f - ms / kMaxMs) };
        };
        for (int age = 1; age < history.size(); ++age)
        {
            DrawLineV(pointAt(age - 1), pointAt(age), textColor());
        }
    }

    void drawBreakdown(Column& col, UiState& ui, const UiStats& stats)
    {
        col.section("Dernière image (ms)");
        if (stats.history == nullptr || stats.history->size() == 0)
        {
            return;
        }

        const FrameRecord& last = stats.history->at(stats.history->size() - 1);
        const float restMs = last.frameMs - last.stepMs - last.drawMs;
        col.pair(TextFormat("souris %.2f", last.step.mouseMs), TextFormat("grille %.2f", last.step.gridMs));
        col.pair(TextFormat("collis. %.2f", last.step.collideMs), TextFormat("déplac. %.2f", last.step.moveMs));
        col.pair(TextFormat("copie %.2f", last.step.copyMs), TextFormat("dessin %.2f", last.drawMs));
        col.pair(TextFormat("reste %.2f", restMs), TextFormat("image %.2f", last.frameMs));

        drawFrameGraph(col.row(44.0f), *stats.history);
        const char* buttonText = stats.message != nullptr ? stats.message : "Exporter CSV (M)";
        if (GuiButton(col.row(), buttonText))
        {
            ui.exportRequested = true;
        }
    }
}

void initUi()
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, kTextSize);
}

void drawPanel(UiState& ui, const UiStats& stats)
{
    DrawRectangle(0, 0, kPanelWidth, kWindowHeight, GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
    DrawLine(kPanelWidth - 1, 0, kPanelWidth - 1, kWindowHeight, GetColor(GuiGetStyle(DEFAULT, LINE_COLOR)));

    Column col;
    drawCountControls(col, ui);
    drawEngineControls(col, ui);
    drawPhysicsControls(col, ui, stats);
    drawMouseControls(col, ui);
    drawStats(col, stats);
    drawBreakdown(col, ui, stats);
}

void requestCount(UiState& ui, int count)
{
    ui.count = count;
    ui.pendingCount = count;
    ui.countEditing = false;
}

bool isTyping(const UiState& ui)
{
    return ui.countEditing;
}

bool isOverPanel(Vector2 point)
{
    return point.x < float(kPanelWidth);
}