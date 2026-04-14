#pragma once
#include <JuceHeader.h>

class CustomDial : public juce::LookAndFeel_V4
{
public:
    CustomDial()
    {
        // Настраиваем глобальные цвета для всех слайдеров, использующих этот стиль
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.3f));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override
    {
        // 1. Фикс размытия: работаем с целыми числами для центра
        auto centreX = (float)x + (float)width  * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;

        // Округляем радиус, чтобы избежать дробных границ
        auto bounds = juce::Rectangle<int>(x, y, width, height).reduced(5);
        auto radius = (float)juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        radius = std::floor(radius); // Убираем дробную часть

        auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // --- 2. РИСУЕМ НАСЕЧКИ (TICKS) ---
        const int numTicks = 11; // Количество черточек
        for (int i = 0; i < numTicks; ++i)
        {
            // Вычисляем угол для каждой насечки
            auto angle = rotaryStartAngle + i * (rotaryEndAngle - rotaryStartAngle) / (numTicks - 1);

            // Линии насечек
            juce::Path tick;
            float tickLen = 3.0f;
            float innerRadius = radius + 2.0f; // Чуть снаружи крутилки

            tick.startNewSubPath(centreX, centreY - innerRadius);
            tick.lineTo(centreX, centreY - innerRadius - tickLen);

            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.strokePath(tick, juce::PathStrokeType(1.5f), juce::AffineTransform::rotation(angle, centreX, centreY));
        }

        // --- 3. ТЕЛО КРУТИЛКИ (С ЧЕТКИМИ КРАЯМИ) ---
        auto dialRadius = radius - 6.0f; // Уменьшаем, чтобы влезли насечки
        auto dialBounds = juce::Rectangle<float>(centreX - dialRadius, centreY - dialRadius,
                                                 dialRadius * 2, dialRadius * 2);

        // Тени и Градиент (как в прошлом примере)
        juce::ColourGradient grad(juce::Colours::grey.darker(0.1f), centreX, dialBounds.getY(),
                                  juce::Colours::black, centreX, dialBounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillEllipse(dialBounds);

        // Четкий контур (черный ободок)
        g.setColour(juce::Colours::black);
        g.drawEllipse(dialBounds, 1.0f);

        // Блик сверху (делает форму объемной)
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawEllipse(dialBounds.reduced(1.0f), 1.0f);

        // --- 4. ИНДИКАТОР ---
        juce::Path p;
        p.addRoundedRectangle(-1.5f, -dialRadius, 3.0f, dialRadius * 0.5f, 1.0f);
        g.setColour(juce::Colours::orange);
        g.fillPath(p, juce::AffineTransform::rotation(toAngle).translated(centreX, centreY));
    }
                          // Кастомизация текстового поля
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        if (!label.isBeingEdited())
        {
            g.fillAll(label.findColour(juce::Label::backgroundColourId));
            g.setColour(label.findColour(juce::Label::textColourId));
            g.setFont(juce::Font("Consolas", 13.0f, juce::Font::plain)); // "Прогерский" шрифт для точности

            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
            g.drawText(label.getText(), textArea, juce::Justification::centred, true);

            // Тонкое подчеркивание
            g.setColour(juce::Colours::orange.withAlpha(0.4f));
            g.fillRect(label.getLocalBounds().removeFromBottom(1));
        }
    }

};
