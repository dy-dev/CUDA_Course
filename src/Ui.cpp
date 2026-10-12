#include "Ui.h"

#include "Params.h"

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
    constexpr float kRowGap = 6.0f;
    constexpr int kTextSize = 16;

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
            GuiLabel(row(18.0f), text);
        }
    };

    int roundToThousand(float value)
    {
        return int(std::lround(value / 1000.0f)) * 1000;
    }

    void drawCountControls(Column& col, UiState& ui)
    {
        col.section("Particules");

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
        col.label("Touches 1 à 5 :");
        col.label("1k, 10k, 50k, 100k, 200k");
    }

    void drawStats(Column& col, const UiStats& stats)
    {
        col.section("Mesures");
        col.label(TextFormat("FPS : %d", stats.fps));
        col.label(TextFormat("N : %d", stats.count));
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
    drawStats(col, stats);
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
