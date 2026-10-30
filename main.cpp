#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr unsigned int WindowWidth = 960;
constexpr unsigned int WindowHeight = 600;
constexpr float GroundY = 500.f;
constexpr float PlayerX = 145.f;
constexpr float PlayerWidth = 48.f;
constexpr float PlayerHeight = 72.f;
constexpr float LevelDuration = 12.f;
constexpr unsigned int TotalLevels = 10;
constexpr unsigned int SampleRate = 44100;
constexpr float Pi = 3.14159265358979323846f;

struct Obstacle
{
    float x;
    float width;
    float height;
};

struct DinoPalette
{
    const char* name;
    sf::Color body;
    sf::Color head;
    sf::Color belly;
    sf::Color detail;
};

enum class Screen
{
    MainMenu,
    Instructions,
    Game
};

bool createTone(sf::SoundBuffer& buffer,
                const std::vector<std::pair<float, float>>& notes,
                float secondsPerNote,
                float volume)
{
    std::vector<std::int16_t> samples;
    const auto samplesPerNote = static_cast<std::size_t>(SampleRate * secondsPerNote);
    samples.reserve(samplesPerNote * notes.size());

    for (const auto& [startFrequency, endFrequency] : notes)
    {
        for (std::size_t i = 0; i < samplesPerNote; ++i)
        {
            const float time = static_cast<float>(i) / static_cast<float>(SampleRate);
            const float progress = static_cast<float>(i) / static_cast<float>(samplesPerNote);
            const float frequency = startFrequency + (endFrequency - startFrequency) * progress;
            const float envelope = std::min(1.f, time * 35.f) *
                                   std::min(1.f, (secondsPerNote - time) * 12.f);
            const float fundamental = std::sin(2.f * Pi * frequency * time);
            const float harmonic = 0.12f * std::sin(4.f * Pi * frequency * time);
            const float value = (fundamental + harmonic) * envelope * volume * 32767.f;
            samples.push_back(static_cast<std::int16_t>(std::clamp(value, -32767.f, 32767.f)));
        }
    }

    return buffer.loadFromSamples(samples.data(),
                                  samples.size(),
                                  1,
                                  SampleRate,
                                  {sf::SoundChannel::Mono});
}

bool createRunningSound(sf::SoundBuffer& buffer)
{
    constexpr float duration = 1.f;
    constexpr std::size_t sampleCount = static_cast<std::size_t>(SampleRate * duration);
    constexpr std::array<float, 4> stepTimes{0.f, 0.25f, 0.5f, 0.75f};
    std::vector<float> soundSamples(sampleCount);
    float peak = 0.f;

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const float time = static_cast<float>(i) / static_cast<float>(SampleRate);
        float sound = 0.f;
        for (std::size_t step = 0; step < stepTimes.size(); ++step)
        {
            const float localTime = time - stepTimes[step];
            if (localTime < 0.f || localTime > 0.2f)
                continue;

            const float attack = std::min(1.f, localTime / 0.004f);
            const float decay = std::exp(-localTime * 25.f);
            const float pitch = 125.f + static_cast<float>(step % 2) * 18.f;
            const float thudPhase = 2.f * Pi * (pitch * localTime - 100.f * localTime * localTime);
            const float tapPhase = 2.f * Pi * (520.f * localTime - 750.f * localTime * localTime);
            const float thud = std::sin(thudPhase) + 0.28f * std::sin(thudPhase * 2.f);
            const float tap = std::sin(tapPhase);
            sound += (thud * 0.68f + tap * 0.32f) * attack * decay * 0.3f;
        }

        soundSamples[i] = sound;
        peak = std::max(peak, std::abs(sound));
    }

    if (peak <= 0.f)
        return false;

    std::vector<std::int16_t> samples;
    samples.reserve(sampleCount);
    const float gain = 0.42f / peak;
    for (float sample : soundSamples)
        samples.push_back(static_cast<std::int16_t>(std::clamp(sample * gain, -0.42f, 0.42f) * 32767.f));

    return buffer.loadFromSamples(samples.data(),
                                  samples.size(),
                                  1,
                                  SampleRate,
                                  {sf::SoundChannel::Mono});
}

bool createCrowdCheer(sf::SoundBuffer& buffer)
{
    constexpr float duration = 4.f;
    constexpr std::size_t sampleCount = static_cast<std::size_t>(SampleRate * duration);
    constexpr std::size_t clapLength = SampleRate / 55;
    constexpr float TwoPi = 2.f * Pi;

    struct Voice
    {
        float frequency;
        float phase;
        float phraseOffset;
        float volume;
    };

    std::mt19937 randomEngine(4317);
    std::uniform_real_distribution<float> pitchDistribution(145.f, 335.f);
    std::uniform_real_distribution<float> phaseDistribution(0.f, TwoPi);
    std::uniform_real_distribution<float> phraseDistribution(0.f, 1.f);
    std::uniform_real_distribution<float> volumeDistribution(0.35f, 0.9f);
    std::uniform_real_distribution<float> clapInterval(0.09f, 0.28f);
    std::uniform_real_distribution<float> clapStrength(0.3f, 0.85f);
    std::uniform_real_distribution<float> noiseDistribution(-1.f, 1.f);

    std::array<Voice, 14> voices;
    for (Voice& voice : voices)
        voice = {pitchDistribution(randomEngine),
                 phaseDistribution(randomEngine),
                 phraseDistribution(randomEngine),
                 volumeDistribution(randomEngine)};

    std::vector<float> samples(sampleCount, 0.f);
    std::size_t clapPosition = 0;
    while (clapPosition < sampleCount)
    {
        clapPosition += static_cast<std::size_t>(clapInterval(randomEngine) * SampleRate);
        const float strength = clapStrength(randomEngine);
        for (std::size_t i = 0; i < clapLength && clapPosition + i < sampleCount; ++i)
        {
            const float decay = std::exp(-static_cast<float>(i) / 180.f);
            samples[clapPosition + i] += noiseDistribution(randomEngine) * decay * strength * 0.38f;
        }
    }

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const float time = static_cast<float>(i) / static_cast<float>(SampleRate);
        float cheer = 0.f;

        for (const Voice& voice : voices)
        {
            const float phrase = TwoPi * (time / 2.7f + voice.phraseOffset);
            const float swell = 0.18f + 0.82f * std::max(0.f, std::sin(phrase));
            const float pitchGlide = voice.frequency * (1.f + 0.14f * std::sin(phrase * 0.5f));
            const float phase = TwoPi * pitchGlide * time + voice.phase;
            const float vowel = std::sin(phase) +
                                0.44f * std::sin(phase * 2.04f) +
                                0.19f * std::sin(phase * 3.08f);
            cheer += vowel * swell * voice.volume;
        }

        const float fadeIn = std::min(1.f, time / 0.12f);
        const float fadeOut = std::min(1.f, (duration - time) / 0.12f);
        samples[i] += cheer * 0.075f * fadeIn * fadeOut;
    }

    std::vector<std::int16_t> pcmSamples;
    pcmSamples.reserve(sampleCount);
    for (float sample : samples)
    {
        const float normalized = std::clamp(sample, -0.95f, 0.95f);
        pcmSamples.push_back(static_cast<std::int16_t>(normalized * 32767.f));
    }

    return buffer.loadFromSamples(pcmSamples.data(),
                                  pcmSamples.size(),
                                  1,
                                  SampleRate,
                                  {sf::SoundChannel::Mono});
}

void drawText(sf::RenderWindow& window,
              const sf::Font& font,
              const std::string& value,
              unsigned int size,
              sf::Vector2f position,
              sf::Color color = sf::Color::White)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    text.setPosition(position);
    window.draw(text);
}

void drawCenteredText(sf::RenderWindow& window,
                     const sf::Font& font,
                     const std::string& value,
                     unsigned int size,
                     sf::Vector2f center,
                     sf::Color color = sf::Color::White)
{
    sf::Text text(font, value, size);
    text.setFillColor(color);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x * 0.5f,
                    bounds.position.y + bounds.size.y * 0.5f});
    text.setPosition(center);
    window.draw(text);
}

template <std::size_t PointCount>
void drawPolygon(sf::RenderWindow& window,
                 const std::array<sf::Vector2f, PointCount>& points,
                 sf::Color color)
{
    sf::ConvexShape polygon(PointCount);
    polygon.setFillColor(color);

    for (std::size_t i = 0; i < PointCount; ++i)
        polygon.setPoint(i, points[i]);

    window.draw(polygon);
}

struct Point3D
{
    float x;
    float y;
    float z;
};

struct Face3D
{
    std::vector<Point3D> points;
    sf::Color color;
    float depth;
};

sf::Vector2f project3D(Point3D point)
{
    constexpr float cameraDistance = 620.f;
    const float scale = cameraDistance / (cameraDistance + point.z);
    return {480.f + (point.x - 480.f) * scale,
            300.f + (point.y - 300.f) * scale};
}

sf::Color shaded(sf::Color color, float brightness)
{
    return {static_cast<std::uint8_t>(std::min(255.f, color.r * brightness)),
            static_cast<std::uint8_t>(std::min(255.f, color.g * brightness)),
            static_cast<std::uint8_t>(std::min(255.f, color.b * brightness)),
            color.a};
}

void drawFace3D(sf::RenderWindow& window, const Face3D& face)
{
    sf::ConvexShape polygon(face.points.size());
    polygon.setFillColor(face.color);
    for (std::size_t i = 0; i < face.points.size(); ++i)
        polygon.setPoint(i, project3D(face.points[i]));
    window.draw(polygon);
}

void drawCuboid3D(sf::RenderWindow& window,
                  float centerX,
                  float centerY,
                  float centerZ,
                  float width,
                  float height,
                  float depth,
                  sf::Color color)
{
    const float left = centerX - width * 0.5f;
    const float right = centerX + width * 0.5f;
    const float top = centerY - height * 0.5f;
    const float bottom = centerY + height * 0.5f;
    const float front = centerZ - depth * 0.5f;
    const float back = centerZ + depth * 0.5f;

    std::array<Face3D, 6> faces{{
        {{{{left, top, front}, {right, top, front}, {right, bottom, front}, {left, bottom, front}}},
         shaded(color, 0.88f), front},
        {{{{right, top, back}, {left, top, back}, {left, bottom, back}, {right, bottom, back}}},
         shaded(color, 0.56f), back},
        {{{{left, top, back}, {left, top, front}, {left, bottom, front}, {left, bottom, back}}},
         shaded(color, 0.68f), (front + back) * 0.5f},
        {{{{right, top, front}, {right, top, back}, {right, bottom, back}, {right, bottom, front}}},
         shaded(color, 0.76f), (front + back) * 0.5f},
        {{{{left, top, back}, {right, top, back}, {right, top, front}, {left, top, front}}},
         shaded(color, 1.16f), back},
        {{{{left, bottom, front}, {right, bottom, front}, {right, bottom, back}, {left, bottom, back}}},
         shaded(color, 0.48f), back},
    }};
    std::sort(faces.begin(), faces.end(), [](const Face3D& lhs, const Face3D& rhs)
              { return lhs.depth > rhs.depth; });
    for (const Face3D& face : faces)
        drawFace3D(window, face);
}

void drawPyramid3D(sf::RenderWindow& window,
                   float centerX,
                   float baseY,
                   float centerZ,
                   float width,
                   float depth,
                   float height,
                   sf::Color color)
{
    const float left = centerX - width * 0.5f;
    const float right = centerX + width * 0.5f;
    const float front = centerZ - depth * 0.5f;
    const float back = centerZ + depth * 0.5f;
    const Point3D tip{centerX, baseY - height, centerZ};
    std::array<Face3D, 4> faces{{
        {{{tip, {left, baseY, front}, {right, baseY, front}}}, shaded(color, 0.82f), front},
        {{{tip, {right, baseY, front}, {right, baseY, back}}}, shaded(color, 0.62f), (front + back) * 0.5f},
        {{{tip, {right, baseY, back}, {left, baseY, back}}}, shaded(color, 0.48f), back},
        {{{tip, {left, baseY, back}, {left, baseY, front}}}, shaded(color, 1.12f), (front + back) * 0.5f},
    }};
    std::sort(faces.begin(), faces.end(), [](const Face3D& lhs, const Face3D& rhs)
              { return lhs.depth > rhs.depth; });
    for (const Face3D& face : faces)
        drawFace3D(window, face);
}

void drawMountain3D(sf::RenderWindow& window,
                    float centerX,
                    float baseY,
                    float centerZ,
                    float width,
                    float height,
                    float peakOffset,
                    sf::Color color,
                    bool snowy)
{
    const float front = centerZ - width * 0.12f;
    const float back = centerZ + width * 0.12f;
    const float leftX = centerX - width * 0.5f;
    const float rightX = centerX + width * 0.5f;
    const float peakX = centerX + peakOffset;
    const Point3D peak{peakX, baseY - height, centerZ};
    const Point3D frontLeft{leftX, baseY, front};
    const Point3D frontRight{rightX, baseY, front};
    const Point3D backLeft{leftX, baseY, back};
    const Point3D backRight{rightX, baseY, back};
    const Point3D frontCenter{centerX, baseY, front};

    std::array<Face3D, 5> faces{{
        {{{peak, backLeft, backRight}}, shaded(color, 0.7f), back},
        {{{peak, backLeft, frontLeft}}, shaded(color, 0.92f), (front + back) * 0.5f},
        {{{peak, backRight, frontRight}}, shaded(color, 0.56f), (front + back) * 0.5f},
        {{{peak, frontLeft, frontCenter}}, shaded(color, 1.12f), front},
        {{{peak, frontCenter, frontRight}}, shaded(color, 0.82f), front},
    }};
    std::sort(faces.begin(), faces.end(), [](const Face3D& lhs, const Face3D& rhs)
              { return lhs.depth > rhs.depth; });
    for (const Face3D& face : faces)
        drawFace3D(window, face);

    if (snowy)
    {
        const float snowlineY = peak.y + height * 0.3f;
        const float snowHalfWidth = width * 0.15f;
        const Point3D snowLeft{peakX - snowHalfWidth, snowlineY, front - 0.5f};
        const Point3D snowCenter{peakX, snowlineY + height * 0.08f, front - 0.5f};
        const Point3D snowRight{peakX + snowHalfWidth, snowlineY, front - 0.5f};
        drawFace3D(window, {{{peak, snowLeft, snowCenter}},
                            sf::Color(236, 243, 235), front - 0.5f});
        drawFace3D(window, {{{peak, snowCenter, snowRight}},
                            sf::Color(255, 255, 247), front - 0.5f});
    }
}

void drawTrianglePrism3D(sf::RenderWindow& window,
                         std::array<Point3D, 3> front,
                         float depth,
                         sf::Color color)
{
    std::array<Point3D, 3> back = front;
    for (Point3D& point : front)
        point.z -= depth * 0.5f;
    for (Point3D& point : back)
        point.z += depth * 0.5f;

    std::array<Face3D, 5> faces{{
        {{{front[0], front[1], front[2]}}, shaded(color, 0.9f), front[0].z},
        {{{back[2], back[1], back[0]}}, shaded(color, 0.62f), back[0].z},
        {{{front[0], back[0], back[1], front[1]}}, shaded(color, 0.72f),
         (front[0].z + back[0].z) * 0.5f},
        {{{front[1], back[1], back[2], front[2]}}, shaded(color, 0.58f),
         (front[1].z + back[1].z) * 0.5f},
        {{{front[2], back[2], back[0], front[0]}}, shaded(color, 1.08f),
         (front[2].z + back[2].z) * 0.5f},
    }};
    std::sort(faces.begin(), faces.end(), [](const Face3D& lhs, const Face3D& rhs)
              { return lhs.depth > rhs.depth; });
    for (const Face3D& face : faces)
        drawFace3D(window, face);
}

void drawMarker3D(sf::RenderWindow& window, Point3D position, float radius, sf::Color color)
{
    const sf::Vector2f screenPosition = project3D(position);
    sf::CircleShape marker(radius * 620.f / (620.f + position.z));
    marker.setOrigin({marker.getRadius(), marker.getRadius()});
    marker.setPosition(screenPosition);
    marker.setFillColor(color);
    window.draw(marker);
}

void drawCloud(sf::RenderWindow& window, float x, float y, float scale)
{
    const sf::Color color(242, 249, 255, 225);
    const std::array<std::pair<sf::Vector2f, float>, 5> puffs{{
        {sf::Vector2f{0.f, 13.f}, 17.f},
        {sf::Vector2f{19.f, 4.f}, 23.f},
        {sf::Vector2f{44.f, 0.f}, 28.f},
        {sf::Vector2f{69.f, 8.f}, 21.f},
        {sf::Vector2f{34.f, 17.f}, 22.f},
    }};
    for (const auto& [offset, radius] : puffs)
    {
        sf::CircleShape puff(radius * scale);
        puff.setPosition({x + offset.x * scale, y + offset.y * scale});
        puff.setFillColor(color);
        window.draw(puff);
    }
}

void drawStar(sf::RenderWindow& window, sf::Vector2f center, float radius, sf::Color color)
{
    sf::ConvexShape star(10);
    star.setPosition(center);
    star.setFillColor(color);
    for (std::size_t point = 0; point < 10; ++point)
    {
        const float angle = -Pi * 0.5f + static_cast<float>(point) * Pi / 5.f;
        const float pointRadius = point % 2 == 0 ? radius : radius * 0.43f;
        star.setPoint(point,
                      {radius + std::cos(angle) * pointRadius,
                       radius + std::sin(angle) * pointRadius});
    }
    window.draw(star);
}

void drawDinoBuddy(sf::RenderWindow& window,
                   sf::Vector2f center,
                   sf::Color bodyColor,
                   sf::Color bellyColor)
{
    drawPolygon(window,
                std::array{sf::Vector2f{center.x - 20.f, center.y - 14.f},
                           sf::Vector2f{center.x - 22.f, center.y - 29.f},
                           sf::Vector2f{center.x - 8.f, center.y - 20.f}},
                bodyColor);
    drawPolygon(window,
                std::array{sf::Vector2f{center.x - 5.f, center.y - 18.f},
                           sf::Vector2f{center.x + 1.f, center.y - 34.f},
                           sf::Vector2f{center.x + 9.f, center.y - 17.f}},
                bodyColor);
    drawPolygon(window,
                std::array{sf::Vector2f{center.x + 8.f, center.y - 14.f},
                           sf::Vector2f{center.x + 22.f, center.y - 24.f},
                           sf::Vector2f{center.x + 19.f, center.y - 5.f}},
                bodyColor);

    sf::CircleShape head(24.f);
    head.setOrigin({24.f, 24.f});
    head.setPosition(center);
    head.setFillColor(bodyColor);
    head.setOutlineThickness(2.f);
    head.setOutlineColor(sf::Color(255, 255, 255, 235));
    window.draw(head);

    sf::CircleShape muzzle(13.f);
    muzzle.setOrigin({13.f, 9.f});
    muzzle.setPosition({center.x + 9.f, center.y + 8.f});
    muzzle.setFillColor(bellyColor);
    window.draw(muzzle);

    sf::CircleShape eyeWhite(5.f);
    eyeWhite.setPosition({center.x + 4.f, center.y - 10.f});
    eyeWhite.setFillColor(sf::Color::White);
    window.draw(eyeWhite);
    sf::CircleShape pupil(2.5f);
    pupil.setPosition({center.x + 6.f, center.y - 8.f});
    pupil.setFillColor(sf::Color(68, 61, 93));
    window.draw(pupil);

    sf::CircleShape cheek(3.5f);
    cheek.setPosition({center.x + 15.f, center.y + 4.f});
    cheek.setFillColor(sf::Color(255, 152, 187, 220));
    window.draw(cheek);

    sf::RectangleShape smile({8.f, 2.f});
    smile.setPosition({center.x + 9.f, center.y + 13.f});
    smile.setFillColor(sf::Color(105, 75, 85));
    window.draw(smile);
}

void drawBalloon(sf::RenderWindow& window, sf::Vector2f center, float scale, sf::Color color)
{
    sf::CircleShape balloon(15.f * scale);
    balloon.setOrigin({15.f * scale, 17.f * scale});
    balloon.setPosition(center);
    balloon.setScale({0.82f, 1.08f});
    balloon.setFillColor(color);
    balloon.setOutlineThickness(1.5f * scale);
    balloon.setOutlineColor(sf::Color(255, 255, 255, 210));
    window.draw(balloon);

    drawPolygon(window,
                std::array{sf::Vector2f{center.x - 3.f * scale, center.y + 13.f * scale},
                           sf::Vector2f{center.x + 3.f * scale, center.y + 13.f * scale},
                           sf::Vector2f{center.x, center.y + 19.f * scale}},
                color);

    sf::VertexArray string(sf::PrimitiveType::LineStrip, 4);
    string[0].position = {center.x, center.y + 18.f * scale};
    string[1].position = {center.x - 3.f * scale, center.y + 26.f * scale};
    string[2].position = {center.x + 3.f * scale, center.y + 34.f * scale};
    string[3].position = {center.x, center.y + 42.f * scale};
    for (std::size_t i = 0; i < string.getVertexCount(); ++i)
        string[i].color = sf::Color(129, 131, 165, 190);
    window.draw(string);

    sf::CircleShape shine(3.f * scale);
    shine.setPosition({center.x - 7.f * scale, center.y - 7.f * scale});
    shine.setFillColor(sf::Color(255, 255, 255, 220));
    window.draw(shine);
}

void drawPineTree(sf::RenderWindow& window, sf::Vector2f base, float scale, bool alternate)
{
    sf::CircleShape shadow(20.f * scale);
    shadow.setOrigin({20.f * scale, 8.f * scale});
    shadow.setPosition({base.x, base.y - 2.f * scale});
    shadow.setScale({1.35f, 0.55f});
    shadow.setFillColor(sf::Color(24, 67, 45, 85));
    window.draw(shadow);

    drawCuboid3D(window, base.x, base.y - 20.f * scale, 0.f,
                 13.f * scale, 43.f * scale, 18.f * scale, sf::Color(113, 76, 48));
    const sf::Color foliage = alternate ? sf::Color(42, 119, 72) : sf::Color(49, 139, 77);
    drawPyramid3D(window, base.x, base.y - 16.f * scale, 0.f,
                  72.f * scale, 44.f * scale, 78.f * scale, foliage);
    drawPyramid3D(window, base.x, base.y - 45.f * scale, 0.f,
                  56.f * scale, 36.f * scale, 70.f * scale, shaded(foliage, 1.08f));
    drawPyramid3D(window, base.x, base.y - 70.f * scale, 0.f,
                  40.f * scale, 30.f * scale, 60.f * scale, shaded(foliage, 1.16f));
}

void drawHouse3D(sf::RenderWindow& window,
                 float centerX,
                 float groundY,
                 float scale,
                 sf::Color wallColor,
                 sf::Color roofColor)
{
    constexpr float depth = 22.f;
    const float wallWidth = 48.f * scale;
    const float wallHeight = 32.f * scale;
    const float roofWidth = 62.f * scale;
    const float roofHeight = 25.f * scale;
    const float centerY = groundY - wallHeight * 0.5f;
    const float frontZ = -depth * 0.5f - 1.f;

    sf::CircleShape shadow(25.f * scale);
    shadow.setOrigin({25.f * scale, 8.f * scale});
    shadow.setPosition({centerX, groundY});
    shadow.setScale({1.45f, 0.48f});
    shadow.setFillColor(sf::Color(36, 82, 49, 95));
    window.draw(shadow);

    drawCuboid3D(window, centerX, centerY, 0.f,
                 wallWidth, wallHeight, depth, wallColor);
    drawTrianglePrism3D(window,
                         {{{centerX - roofWidth * 0.5f, groundY - wallHeight, 0.f},
                           {centerX, groundY - wallHeight - roofHeight, 0.f},
                           {centerX + roofWidth * 0.5f, groundY - wallHeight, 0.f}}},
                         depth + 5.f, roofColor);

    drawCuboid3D(window, centerX, groundY - 11.f * scale, frontZ,
                 10.f * scale, 21.f * scale, 3.f, sf::Color(116, 72, 49));
    drawCuboid3D(window, centerX - 13.f * scale, groundY - 21.f * scale, frontZ,
                 9.f * scale, 8.f * scale, 3.f, sf::Color(117, 211, 231));
    drawCuboid3D(window, centerX + 13.f * scale, groundY - 21.f * scale, frontZ,
                 9.f * scale, 8.f * scale, 3.f, sf::Color(255, 230, 129));
    drawCuboid3D(window, centerX, groundY - 1.5f * scale, frontZ - 1.f,
                 14.f * scale, 3.f * scale, 4.f, sf::Color(235, 206, 147));
    drawCuboid3D(window, centerX + 15.f * scale, groundY - wallHeight - roofHeight + 10.f * scale,
                 4.f, 7.f * scale, 17.f * scale, 7.f, sf::Color(174, 85, 67));
}

int menuOptionAt(sf::Vector2i position)
{
    constexpr float buttonLeft = 330.f;
    constexpr float buttonWidth = 300.f;
    constexpr float buttonHeight = 52.f;
    constexpr float firstButtonTop = 292.f;
    constexpr float buttonSpacing = 65.f;

    if (position.x < buttonLeft || position.x > buttonLeft + buttonWidth)
        return -1;

    for (int option = 0; option < 3; ++option)
    {
        const float buttonTop = firstButtonTop + static_cast<float>(option) * buttonSpacing;
        if (position.y >= buttonTop && position.y <= buttonTop + buttonHeight)
            return option;
    }

    return -1;
}

int pauseOptionAt(sf::Vector2i position)
{
    constexpr int buttonLeft = 330;
    constexpr int buttonRight = 630;
    constexpr int firstButtonTop = 282;
    constexpr int buttonHeight = 48;
    constexpr int buttonSpacing = 62;
    if (position.x < buttonLeft || position.x > buttonRight)
        return -1;

    for (int option = 0; option < 3; ++option)
    {
        const int top = firstButtonTop + option * buttonSpacing;
        if (position.y >= top && position.y <= top + buttonHeight)
            return option;
    }
    return -1;
}

int exitOptionAt(sf::Vector2i position)
{
    constexpr int buttonTop = 350;
    constexpr int buttonBottom = 402;
    if (position.y < buttonTop || position.y > buttonBottom)
        return -1;
    if (position.x >= 310 && position.x <= 470)
        return 0;
    if (position.x >= 490 && position.x <= 650)
        return 1;
    return -1;
}

int dinoOptionAt(sf::Vector2i position)
{
    constexpr float firstCardLeft = 267.f;
    constexpr float cardWidth = 132.f;
    constexpr float cardSpacing = 147.f;
    constexpr float cardTop = 212.f;
    constexpr float cardHeight = 56.f;

    if (position.y < cardTop || position.y > cardTop + cardHeight)
        return -1;

    for (int dino = 0; dino < 3; ++dino)
    {
        const float cardLeft = firstCardLeft + static_cast<float>(dino) * cardSpacing;
        if (position.x >= cardLeft && position.x <= cardLeft + cardWidth)
            return dino;
    }

    return -1;
}
}

int main()
{
    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf") &&
        !font.openFromFile("C:/Windows/Fonts/segoeui.ttf"))
    {
        std::cerr << "No se encontro Arial ni Segoe UI en C:\\Windows\\Fonts.\n";
        return 1;
    }

    sf::SoundBuffer jumpBuffer;
    sf::SoundBuffer hitBuffer;
    sf::SoundBuffer levelBuffer;
    sf::SoundBuffer crowdCheerBuffer;
    sf::SoundBuffer runningBuffer;
    if (!createTone(jumpBuffer, {{520.f, 820.f}}, 0.16f, 0.36f) ||
        !createTone(hitBuffer, {{190.f, 90.f}}, 0.24f, 0.34f) ||
        !createTone(levelBuffer, {{523.f, 523.f}, {659.f, 659.f}, {784.f, 784.f}}, 0.15f, 0.32f) ||
        !createCrowdCheer(crowdCheerBuffer) ||
        !createRunningSound(runningBuffer))
    {
        std::cerr << "No se pudieron preparar los efectos de sonido y las porras.\n";
        return 1;
    }

    sf::Sound jumpSound(jumpBuffer);
    sf::Sound hitSound(hitBuffer);
    sf::Sound levelSound(levelBuffer);
    sf::Sound crowdCheerSound(crowdCheerBuffer);
    sf::Sound runningSound(runningBuffer);
    crowdCheerSound.setLooping(true);
    crowdCheerSound.setVolume(22.f);
    runningSound.setLooping(true);
    runningSound.setVolume(30.f);
    sf::RenderWindow window(sf::VideoMode({WindowWidth, WindowHeight}), "Carreritas Dino");
    window.setVerticalSyncEnabled(true);

    const std::array<DinoPalette, 3> dinoPalettes{{
        {"Verde", sf::Color(57, 169, 91), sf::Color(68, 182, 98),
         sf::Color(189, 219, 132), sf::Color(255, 183, 77)},
        {"Rosa", sf::Color(206, 91, 143), sf::Color(235, 125, 171),
         sf::Color(255, 196, 211), sf::Color(255, 197, 92)},
        {"Cafe", sf::Color(139, 91, 55), sf::Color(166, 112, 68),
         sf::Color(220, 185, 132), sf::Color(242, 180, 76)},
    }};
    std::mt19937 randomEngine(std::random_device{}());
    std::vector<Obstacle> obstacles;
    unsigned int level = 1;
    float levelTime = 0.f;
    float playerY = GroundY - PlayerHeight;
    float verticalVelocity = 0.f;
    float spawnTime = 0.f;
    float spawnDelay = 1.15f;
    float groundOffset = 0.f;
    float sceneryDistance = 0.f;
    float cloudOffset = 0.f;
    float animationTime = 0.f;
    bool gameOver = false;
    bool victory = false;
    bool paused = false;
    bool exitConfirmation = false;
    bool muted = false;
    Screen screen = Screen::MainMenu;
    int selectedMenuOption = 0;
    int selectedPauseOption = 0;
    int selectedExitOption = 1;
    int selectedDino = 0;

    auto restartGame = [&]()
    {
        level = 1;
        levelTime = 0.f;
        playerY = GroundY - PlayerHeight;
        verticalVelocity = 0.f;
        spawnTime = 0.f;
        spawnDelay = 1.15f;
        groundOffset = 0.f;
        sceneryDistance = 0.f;
        animationTime = 0.f;
        obstacles.clear();
        gameOver = false;
        victory = false;
        paused = false;
    };

    auto activateMenuOption = [&](int option)
    {
        if (option == 0)
        {
            restartGame();
            screen = Screen::Game;
        }
        else if (option == 1)
            screen = Screen::Instructions;
        else if (option == 2)
            exitConfirmation = true;
    };

    sf::Clock frameClock;
    while (window.isOpen())
    {
        float deltaTime = std::min(frameClock.restart().asSeconds(), 0.05f);
        cloudOffset = std::fmod(cloudOffset + 18.f * deltaTime, 1100.f);

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                exitConfirmation = true;

            if (const auto* mouseEvent = event->getIf<sf::Event::MouseMoved>())
            {
                if (exitConfirmation)
                {
                    const int hoveredExitOption = exitOptionAt(mouseEvent->position);
                    if (hoveredExitOption >= 0)
                        selectedExitOption = hoveredExitOption;
                }
                else if (screen == Screen::Game && paused)
                {
                    const int hoveredPauseOption = pauseOptionAt(mouseEvent->position);
                    if (hoveredPauseOption >= 0)
                        selectedPauseOption = hoveredPauseOption;
                }
                else if (screen == Screen::MainMenu)
                {
                    const int hoveredDino = dinoOptionAt(mouseEvent->position);
                    if (hoveredDino >= 0)
                        selectedDino = hoveredDino;

                    const int hoveredOption = menuOptionAt(mouseEvent->position);
                    if (hoveredOption >= 0)
                        selectedMenuOption = hoveredOption;
                }
            }

            if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouseEvent->button == sf::Mouse::Button::Left && exitConfirmation)
                {
                    const int clickedExitOption = exitOptionAt(mouseEvent->position);
                    if (clickedExitOption == 0)
                        window.close();
                    else if (clickedExitOption == 1)
                        exitConfirmation = false;
                }
                else if (mouseEvent->button == sf::Mouse::Button::Left &&
                         screen == Screen::Game && paused)
                {
                    const int clickedPauseOption = pauseOptionAt(mouseEvent->position);
                    if (clickedPauseOption == 0)
                        paused = false;
                    else if (clickedPauseOption == 1)
                    {
                        paused = false;
                        screen = Screen::MainMenu;
                    }
                    else if (clickedPauseOption == 2)
                        exitConfirmation = true;
                }
                else if (mouseEvent->button == sf::Mouse::Button::Left &&
                         screen == Screen::MainMenu)
                {
                    const int clickedDino = dinoOptionAt(mouseEvent->position);
                    if (clickedDino >= 0)
                        selectedDino = clickedDino;
                    else
                    {
                        const int clickedOption = menuOptionAt(mouseEvent->position);
                        if (clickedOption >= 0)
                            activateMenuOption(clickedOption);
                    }
                }
                else if (mouseEvent->button == sf::Mouse::Button::Left &&
                         screen == Screen::Instructions &&
                         mouseEvent->position.x >= 330 && mouseEvent->position.x <= 630 &&
                         mouseEvent->position.y >= 425 && mouseEvent->position.y <= 483)
                    screen = Screen::MainMenu;
            }

            if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (exitConfirmation)
                {
                    if (keyEvent->code == sf::Keyboard::Key::Escape ||
                        keyEvent->code == sf::Keyboard::Key::N)
                        exitConfirmation = false;
                    else if (keyEvent->code == sf::Keyboard::Key::Y ||
                             keyEvent->code == sf::Keyboard::Key::Enter)
                        window.close();
                    else if (keyEvent->code == sf::Keyboard::Key::Left ||
                             keyEvent->code == sf::Keyboard::Key::Right)
                        selectedExitOption = 1 - selectedExitOption;
                }
                else if (screen == Screen::MainMenu)
                {
                    if (keyEvent->code == sf::Keyboard::Key::Escape)
                        exitConfirmation = true;
                    else if (keyEvent->code == sf::Keyboard::Key::Up ||
                             keyEvent->code == sf::Keyboard::Key::W)
                        selectedMenuOption = (selectedMenuOption + 2) % 3;
                    else if (keyEvent->code == sf::Keyboard::Key::Down ||
                             keyEvent->code == sf::Keyboard::Key::S)
                        selectedMenuOption = (selectedMenuOption + 1) % 3;
                    else if (keyEvent->code == sf::Keyboard::Key::Num1)
                        selectedDino = 0;
                    else if (keyEvent->code == sf::Keyboard::Key::Num2)
                        selectedDino = 1;
                    else if (keyEvent->code == sf::Keyboard::Key::Num3)
                        selectedDino = 2;
                    else if (keyEvent->code == sf::Keyboard::Key::Enter)
                        activateMenuOption(selectedMenuOption);
                    else if (keyEvent->code == sf::Keyboard::Key::M)
                        muted = !muted;
                }
                else if (screen == Screen::Instructions)
                {
                    if (keyEvent->code == sf::Keyboard::Key::Escape ||
                        keyEvent->code == sf::Keyboard::Key::Backspace)
                        screen = Screen::MainMenu;
                    else if (keyEvent->code == sf::Keyboard::Key::Enter)
                    {
                        restartGame();
                        screen = Screen::Game;
                    }
                    else if (keyEvent->code == sf::Keyboard::Key::M)
                        muted = !muted;
                }
                else if (screen == Screen::Game && paused)
                {
                    if (keyEvent->code == sf::Keyboard::Key::Escape ||
                        keyEvent->code == sf::Keyboard::Key::P ||
                        keyEvent->code == sf::Keyboard::Key::Space)
                        paused = false;
                    else if (keyEvent->code == sf::Keyboard::Key::Up ||
                             keyEvent->code == sf::Keyboard::Key::W)
                        selectedPauseOption = (selectedPauseOption + 2) % 3;
                    else if (keyEvent->code == sf::Keyboard::Key::Down ||
                             keyEvent->code == sf::Keyboard::Key::S)
                        selectedPauseOption = (selectedPauseOption + 1) % 3;
                    else if (keyEvent->code == sf::Keyboard::Key::Enter)
                    {
                        if (selectedPauseOption == 0)
                            paused = false;
                        else if (selectedPauseOption == 1)
                        {
                            paused = false;
                            screen = Screen::MainMenu;
                        }
                        else
                            exitConfirmation = true;
                    }
                    else if (keyEvent->code == sf::Keyboard::Key::M)
                        muted = !muted;
                }
                else if (keyEvent->code == sf::Keyboard::Key::Escape)
                    paused = true;
                else if (keyEvent->code == sf::Keyboard::Key::M)
                    muted = !muted;
                else if (gameOver && keyEvent->code == sf::Keyboard::Key::R)
                    restartGame();
                else if (victory && keyEvent->code == sf::Keyboard::Key::Enter)
                    restartGame();
                else if (!gameOver && !victory &&
                         (keyEvent->code == sf::Keyboard::Key::Space ||
                          keyEvent->code == sf::Keyboard::Key::Up ||
                          keyEvent->code == sf::Keyboard::Key::W))
                {
                    if (paused)
                        paused = false;
                    else if (playerY >= GroundY - PlayerHeight)
                    {
                        verticalVelocity = -720.f;
                        if (!muted)
                            jumpSound.play();
                    }
                }
                else if (!gameOver && !victory && keyEvent->code == sf::Keyboard::Key::P)
                {
                    paused = true;
                    selectedPauseOption = 0;
                }
            }
        }

        const bool shouldPlayCrowdCheer =
            screen == Screen::Game && !paused && !gameOver && !victory && !muted;
        if (shouldPlayCrowdCheer && crowdCheerSound.getStatus() != sf::SoundSource::Status::Playing)
            crowdCheerSound.play();
        else if (!shouldPlayCrowdCheer &&
                 crowdCheerSound.getStatus() != sf::SoundSource::Status::Stopped)
            crowdCheerSound.stop();

        const bool shouldPlayRunning = screen == Screen::Game && !paused &&
                                       !gameOver && !victory && !muted;
        if (shouldPlayRunning && runningSound.getStatus() != sf::SoundSource::Status::Playing)
            runningSound.play();
        else if (!shouldPlayRunning &&
                 runningSound.getStatus() != sf::SoundSource::Status::Stopped)
            runningSound.stop();

        if (screen == Screen::Game && !paused && !gameOver && !victory)
        {
            const float speed = 365.f + static_cast<float>(level - 1) * 37.f;
            levelTime += deltaTime;
            animationTime += deltaTime * (7.f + static_cast<float>(level));
            groundOffset = std::fmod(groundOffset + speed * deltaTime, 70.f);
            sceneryDistance += speed * deltaTime;
            playerY += verticalVelocity * deltaTime;
            verticalVelocity += 1750.f * deltaTime;

            if (playerY >= GroundY - PlayerHeight)
            {
                playerY = GroundY - PlayerHeight;
                verticalVelocity = 0.f;
            }

            spawnTime += deltaTime;
            if (spawnTime >= spawnDelay)
            {
                std::uniform_real_distribution<float> widthDistribution(36.f, 54.f);
                std::uniform_real_distribution<float> heightDistribution(
                    42.f + static_cast<float>(level),
                    62.f + static_cast<float>(level) * 2.f);
                std::uniform_real_distribution<float> gapDistribution(
                    620.f - static_cast<float>(level) * 5.f,
                    850.f - static_cast<float>(level) * 5.f);

                obstacles.push_back({static_cast<float>(WindowWidth) + 10.f,
                                     widthDistribution(randomEngine),
                                     heightDistribution(randomEngine)});
                spawnTime = 0.f;
                spawnDelay = gapDistribution(randomEngine) / speed;
            }

            for (Obstacle& obstacle : obstacles)
                obstacle.x -= speed * deltaTime;

            obstacles.erase(
                std::remove_if(obstacles.begin(),
                               obstacles.end(),
                               [](const Obstacle& obstacle) { return obstacle.x + obstacle.width < 0.f; }),
                obstacles.end());

            const float playerLeft = PlayerX + 7.f;
            const float playerRight = PlayerX + PlayerWidth - 7.f;
            const float playerTop = playerY + 5.f;
            const float playerBottom = playerY + PlayerHeight;

            for (const Obstacle& obstacle : obstacles)
            {
                const float obstacleTop = GroundY - obstacle.height;
                if (playerRight > obstacle.x && playerLeft < obstacle.x + obstacle.width &&
                    playerBottom > obstacleTop && playerTop < GroundY)
                {
                    gameOver = true;
                    if (!muted)
                        hitSound.play();
                    break;
                }
            }

            if (levelTime >= LevelDuration)
            {
                if (level == TotalLevels)
                    victory = true;
                else
                {
                    ++level;
                    levelTime = 0.f;
                    spawnTime = 0.f;
                    spawnDelay = 1.05f;
                    obstacles.clear();
                    if (!muted)
                        levelSound.play();
                }
            }
        }

        window.clear(sf::Color(137, 202, 235));

        sf::CircleShape sunGlow(58.f);
        sunGlow.setPosition({759.f, 55.f});
        sunGlow.setFillColor(sf::Color(255, 232, 153, 55));
        window.draw(sunGlow);

        sf::CircleShape sun(36.f);
        sun.setPosition({781.f, 77.f});
        sun.setFillColor(sf::Color(255, 224, 132));
        window.draw(sun);
        sf::CircleShape sunHighlight(25.f);
        sunHighlight.setPosition({786.f, 81.f});
        sunHighlight.setFillColor(sf::Color(255, 239, 177, 190));
        window.draw(sunHighlight);

        drawMountain3D(window, -25.f, 390.f, 330.f, 430.f, 175.f, 12.f,
                       sf::Color(139, 169, 188), true);
        drawMountain3D(window, 190.f, 396.f, 290.f, 470.f, 224.f, 35.f,
                       sf::Color(128, 157, 179), true);
        drawMountain3D(window, 435.f, 392.f, 320.f, 530.f, 190.f, -28.f,
                       sf::Color(141, 168, 184), false);
        drawMountain3D(window, 690.f, 397.f, 300.f, 495.f, 218.f, 24.f,
                       sf::Color(127, 154, 174), true);
        drawMountain3D(window, 950.f, 392.f, 340.f, 520.f, 178.f, -18.f,
                       sf::Color(141, 169, 187), false);

        drawMountain3D(window, 60.f, 409.f, 125.f, 390.f, 126.f, -24.f,
                       sf::Color(105, 150, 153), false);
        drawMountain3D(window, 310.f, 412.f, 105.f, 430.f, 174.f, 30.f,
                       sf::Color(95, 140, 148), false);
        drawMountain3D(window, 580.f, 410.f, 135.f, 440.f, 137.f, -30.f,
                       sf::Color(103, 148, 151), false);
        drawMountain3D(window, 865.f, 412.f, 115.f, 430.f, 165.f, 26.f,
                       sf::Color(93, 138, 146), false);

        const std::array<std::pair<float, float>, 4> cloudPositions{{
            {0.f, 116.f}, {275.f, 91.f}, {550.f, 153.f}, {825.f, 104.f}}};
        for (std::size_t i = 0; i < cloudPositions.size(); ++i)
        {
            const float scale = i == 2 ? 0.72f : 0.88f;
            const float travel = cloudOffset * (i == 2 ? 0.72f : 1.f);
            const float x = std::fmod(cloudPositions[i].first + travel, 1100.f) - 90.f;
            drawCloud(window, x, cloudPositions[i].second, scale);
        }

        for (const sf::Vector2f birdPosition : {sf::Vector2f{277.f, 126.f},
                                                sf::Vector2f{316.f, 155.f},
                                                sf::Vector2f{581.f, 111.f}})
        {
            drawPolygon(window,
                        std::array{sf::Vector2f{birdPosition.x, birdPosition.y + 5.f},
                                   sf::Vector2f{birdPosition.x + 8.f, birdPosition.y},
                                   sf::Vector2f{birdPosition.x + 15.f, birdPosition.y + 6.f},
                                   sf::Vector2f{birdPosition.x + 8.f, birdPosition.y + 3.f}},
                        sf::Color(72, 105, 117, 190));
            drawPolygon(window,
                        std::array{sf::Vector2f{birdPosition.x + 8.f, birdPosition.y + 3.f},
                                   sf::Vector2f{birdPosition.x + 15.f, birdPosition.y + 6.f},
                                   sf::Vector2f{birdPosition.x + 23.f, birdPosition.y + 1.f},
                                   sf::Vector2f{birdPosition.x + 16.f, birdPosition.y + 5.f}},
                        sf::Color(72, 105, 117, 190));
        }

        sf::RectangleShape grass({static_cast<float>(WindowWidth), 260.f});
        grass.setPosition({0.f, 340.f});
        grass.setFillColor(sf::Color(75, 153, 89));
        window.draw(grass);

        drawPolygon(window,
                    std::array{sf::Vector2f{0.f, 393.f}, sf::Vector2f{143.f, 350.f},
                               sf::Vector2f{291.f, 388.f}, sf::Vector2f{446.f, 347.f},
                               sf::Vector2f{603.f, 386.f}, sf::Vector2f{781.f, 348.f},
                               sf::Vector2f{960.f, 389.f}, sf::Vector2f{960.f, 455.f},
                               sf::Vector2f{0.f, 455.f}},
                    sf::Color(68, 139, 78));

        drawHouse3D(window, 74.f, 404.f, 0.88f,
                    sf::Color(239, 133, 91), sf::Color(190, 69, 65));
        drawHouse3D(window, 264.f, 398.f, 0.68f,
                    sf::Color(248, 218, 125), sf::Color(71, 133, 183));
        drawHouse3D(window, 706.f, 401.f, 0.74f,
                    sf::Color(133, 199, 157), sf::Color(208, 103, 74));
        drawHouse3D(window, 900.f, 405.f, 0.91f,
                    sf::Color(212, 148, 212), sf::Color(74, 91, 157));

        for (const sf::Vector2f shrubPosition : {sf::Vector2f{44.f, 361.f},
                                                 sf::Vector2f{123.f, 378.f},
                                                 sf::Vector2f{228.f, 369.f},
                                                 sf::Vector2f{745.f, 371.f},
                                                 sf::Vector2f{865.f, 359.f},
                                                 sf::Vector2f{921.f, 385.f}})
        {
            sf::CircleShape shrub(15.f);
            shrub.setPosition(shrubPosition);
            shrub.setScale({1.6f, 0.8f});
            shrub.setFillColor(sf::Color(46, 116, 67));
            window.draw(shrub);

            sf::CircleShape highlight(7.f);
            highlight.setPosition({shrubPosition.x + 10.f, shrubPosition.y + 2.f});
            highlight.setFillColor(sf::Color(83, 157, 76, 220));
            window.draw(highlight);
        }

        for (const sf::Vector2f flowerPosition : {sf::Vector2f{30.f, 514.f},
                                                  sf::Vector2f{82.f, 551.f},
                                                  sf::Vector2f{218.f, 536.f},
                                                  sf::Vector2f{746.f, 546.f},
                                                  sf::Vector2f{893.f, 520.f},
                                                  sf::Vector2f{936.f, 558.f}})
        {
            sf::RectangleShape stem({2.f, 12.f});
            stem.setPosition({flowerPosition.x + 3.f, flowerPosition.y + 4.f});
            stem.setFillColor(sf::Color(48, 105, 55));
            window.draw(stem);

            sf::CircleShape petal(4.f);
            petal.setPosition(flowerPosition);
            petal.setFillColor(sf::Color(255, 218, 112));
            window.draw(petal);
            petal.setRadius(2.f);
            petal.setPosition({flowerPosition.x + 2.f, flowerPosition.y + 2.f});
            petal.setFillColor(sf::Color(255, 249, 205));
            window.draw(petal);
        }

        constexpr float horizonY = 316.f;
        const float treeOffset = std::fmod(sceneryDistance / 190.f, 1.f);
        std::array<float, 7> treeDepths{};
        for (std::size_t row = 0; row < treeDepths.size(); ++row)
            treeDepths[row] = std::fmod(static_cast<float>(row) / 7.f + treeOffset, 1.f);
        std::sort(treeDepths.begin(), treeDepths.end());

        for (std::size_t row = 0; row < treeDepths.size(); ++row)
        {
            const float depth = treeDepths[row];
            const float treeY = horizonY + depth * 328.f;
            const float treeScale = 0.22f + depth * 0.88f;
            const float roadHalfWidth = 34.f + depth * 610.f;
            const float roadside = 20.f + depth * 18.f;
            drawPineTree(window, {480.f - roadHalfWidth - roadside, treeY},
                         treeScale, row % 2 == 0);
            drawPineTree(window, {480.f + roadHalfWidth + roadside, treeY},
                         treeScale, row % 2 != 0);
        }

        const auto roadEdge = [](float depth, float side, float extra)
        {
            const float halfWidth = 34.f + depth * 610.f + extra;
            return sf::Vector2f{480.f + side * halfWidth, horizonY + depth * 328.f};
        };
        drawPolygon(window,
                    std::array{roadEdge(0.f, -1.f, 20.f), roadEdge(0.f, 1.f, 20.f),
                               roadEdge(1.f, 1.f, 20.f), roadEdge(1.f, -1.f, 20.f)},
                    sf::Color(205, 185, 128));
        drawPolygon(window,
                    std::array{roadEdge(0.f, -1.f, 0.f), roadEdge(0.f, 1.f, 0.f),
                               roadEdge(1.f, 1.f, 0.f), roadEdge(1.f, -1.f, 0.f)},
                    sf::Color(74, 79, 78));

        for (int dash = 0; dash < 11; ++dash)
        {
            const float depth = std::fmod(static_cast<float>(dash) / 11.f +
                                              std::fmod(sceneryDistance / 340.f, 1.f),
                                          1.f);
            const float dashDepth = std::min(depth + 0.035f + depth * 0.045f, 1.f);
            const float halfWidth = 1.5f + depth * 4.f;
            drawPolygon(window,
                        std::array{sf::Vector2f{480.f - halfWidth, horizonY + depth * 328.f},
                                   sf::Vector2f{480.f + halfWidth, horizonY + depth * 328.f},
                                   sf::Vector2f{480.f + halfWidth * 1.3f,
                                                horizonY + dashDepth * 328.f},
                                   sf::Vector2f{480.f - halfWidth * 1.3f,
                                                horizonY + dashDepth * 328.f}},
                        sf::Color(235, 224, 172, 210));
        }

        sf::RectangleShape groundLine({static_cast<float>(WindowWidth), 5.f});
        groundLine.setPosition({0.f, GroundY});
        groundLine.setFillColor(sf::Color(106, 184, 105));
        window.draw(groundLine);

        for (const Obstacle& obstacle : obstacles)
        {
            sf::CircleShape shadow(obstacle.width * 0.46f);
            shadow.setOrigin({obstacle.width * 0.46f, obstacle.width * 0.16f});
            shadow.setPosition({obstacle.x + obstacle.width * 0.55f, GroundY - 2.f});
            shadow.setScale({1.25f, 0.45f});
            shadow.setFillColor(sf::Color(28, 54, 42, 100));
            window.draw(shadow);

            drawCuboid3D(window,
                         obstacle.x + obstacle.width * 0.5f,
                         GroundY - obstacle.height * 0.5f,
                         0.f,
                         obstacle.width,
                         obstacle.height,
                         30.f,
                         sf::Color(218, 112, 63));
            drawCuboid3D(window,
                         obstacle.x + obstacle.width * 0.5f,
                         GroundY - obstacle.height * 0.5f,
                         -16.f,
                         5.f,
                         obstacle.height - 12.f,
                         3.f,
                         sf::Color(250, 190, 105));
        }

        const DinoPalette& palette = dinoPalettes[static_cast<std::size_t>(selectedDino)];
        const bool airborne = playerY < GroundY - PlayerHeight;
        const float legSwing = airborne ? 2.f : std::sin(animationTime) * 5.f;

        sf::CircleShape playerShadow(22.f);
        playerShadow.setOrigin({22.f, 8.f});
        playerShadow.setPosition({PlayerX + PlayerWidth * 0.5f,
                                  GroundY - 2.f + (GroundY - PlayerHeight - playerY) * 0.12f});
        playerShadow.setScale({1.25f, 0.38f});
        playerShadow.setFillColor(sf::Color(25, 55, 39, 105));
        window.draw(playerShadow);

        drawTrianglePrism3D(window,
                            {{{PlayerX + 5.f, playerY + 39.f, 0.f},
                              {PlayerX + 20.f, playerY + 31.f, 0.f},
                              {PlayerX + 21.f, playerY + 44.f, 0.f}}},
                            20.f, palette.body);
        drawCuboid3D(window, PlayerX + 25.f, playerY + 45.f, 0.f,
                     32.f, 27.f, 34.f, palette.body);
        drawCuboid3D(window, PlayerX + 29.f, playerY + 49.f, -19.f,
                     17.f, 12.f, 4.f, palette.belly);
        drawCuboid3D(window, PlayerX + 34.f, playerY + 29.f, 0.f,
                     14.f, 26.f, 24.f, palette.body);
        drawCuboid3D(window, PlayerX + 36.f, playerY + 17.f, 0.f,
                     25.f, 22.f, 29.f, palette.head);
        drawCuboid3D(window, PlayerX + 45.f, playerY + 20.f, -2.f,
                     14.f, 10.f, 24.f, palette.head);
        drawTrianglePrism3D(window,
                            {{{PlayerX + 16.f, playerY + 34.f, 0.f},
                              {PlayerX + 21.f, playerY + 23.f, 0.f},
                              {PlayerX + 27.f, playerY + 35.f, 0.f}}},
                            18.f, palette.detail);
        drawCuboid3D(window, PlayerX + 36.f, playerY + 41.f, -20.f,
                     12.f, 6.f, 5.f, palette.head);

        drawMarker3D(window, {PlayerX + 41.f, playerY + 15.f, -16.f},
                     3.f, sf::Color::White);
        drawMarker3D(window, {PlayerX + 42.f, playerY + 15.f, -19.f},
                     1.7f, sf::Color(35, 44, 37));
        drawMarker3D(window, {PlayerX + 51.f, playerY + 22.f, -15.f},
                     1.6f, sf::Color(35, 92, 54));

        drawCuboid3D(window, PlayerX + 19.f, playerY + 60.f + legSwing, 0.f,
                     9.f, 16.f, 17.f, palette.body);
        drawCuboid3D(window, PlayerX + 34.f, playerY + 60.f - legSwing, 0.f,
                     9.f, 16.f, 17.f, palette.body);
        drawCuboid3D(window, PlayerX + 18.f, playerY + 68.f + legSwing, -2.f,
                     14.f, 5.f, 19.f, palette.detail);
        drawCuboid3D(window, PlayerX + 34.f, playerY + 68.f - legSwing, -2.f,
                     14.f, 5.f, 19.f, palette.detail);

        sf::RectangleShape hud({WindowWidth, 78.f});
        hud.setFillColor(sf::Color(21, 39, 67, 225));
        window.draw(hud);
        drawText(window, font, "CARRERITAS DINO", 22, {24.f, 15.f});
        drawText(window,
                 font,
                 "NIVEL " + std::to_string(level) + " / " + std::to_string(TotalLevels),
                 22,
                 {245.f, 17.f},
                 sf::Color(255, 222, 125));
        drawText(window,
                 font,
                 "DINOSAURIO: " + std::string(palette.name),
                 14,
                 {430.f, 21.f},
                 sf::Color(207, 235, 201));
        drawText(window, font, "SALTAR: ESPACIO / W / ARRIBA  PAUSA: P  SONIDO: M", 12, {610.f, 22.f});

        sf::RectangleShape progressBackground({WindowWidth - 48.f, 8.f});
        progressBackground.setPosition({24.f, 61.f});
        progressBackground.setFillColor(sf::Color(69, 84, 105));
        window.draw(progressBackground);

        sf::RectangleShape progressFill({(WindowWidth - 48.f) * std::min(levelTime / LevelDuration, 1.f), 8.f});
        progressFill.setPosition({24.f, 61.f});
        progressFill.setFillColor(sf::Color(104, 221, 157));
        window.draw(progressFill);

        if (muted)
            drawText(window, font, "SONIDO: DESACTIVADO", 14, {24.f, 548.f}, sf::Color(255, 230, 160));

        if (paused || gameOver || victory)
        {
            sf::RectangleShape shade({static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)});
            shade.setFillColor(sf::Color(13, 23, 40, 190));
            window.draw(shade);
        }

        if (paused)
        {
            sf::RectangleShape pausePanel({460.f, 270.f});
            pausePanel.setPosition({250.f, 175.f});
            pausePanel.setFillColor(sf::Color(255, 246, 252, 248));
            pausePanel.setOutlineThickness(4.f);
            pausePanel.setOutlineColor(sf::Color(230, 160, 211));
            window.draw(pausePanel);
            drawCenteredText(window, font, "¡Tomemos un descanso!", 34, {480.f, 225.f},
                             sf::Color(129, 83, 171));

            const std::array<std::string, 3> pauseLabels{
                "CONTINUAR", "VOLVER AL MENU", "SALIR DEL JUEGO"};
            const std::array<sf::Color, 3> pauseColors{
                sf::Color(255, 191, 218), sf::Color(202, 180, 239), sf::Color(169, 220, 244)};
            for (int option = 0; option < static_cast<int>(pauseLabels.size()); ++option)
            {
                sf::RectangleShape button({300.f, 48.f});
                button.setPosition({330.f, 282.f + static_cast<float>(option) * 62.f});
                button.setFillColor(pauseColors[static_cast<std::size_t>(option)]);
                button.setOutlineThickness(option == selectedPauseOption ? 4.f : 2.f);
                button.setOutlineColor(option == selectedPauseOption
                                           ? sf::Color(135, 93, 177)
                                           : sf::Color::White);
                window.draw(button);
                drawCenteredText(window, font, pauseLabels[static_cast<std::size_t>(option)],
                                 18, {480.f, 306.f + static_cast<float>(option) * 62.f},
                                 sf::Color(82, 77, 123));
            }
            drawCenteredText(window, font, "P o Escape: continuar", 14, {480.f, 446.f},
                             sf::Color(102, 111, 150));
        }
        else if (gameOver)
        {
            drawText(window, font, "FIN DE LA PARTIDA", 48, {267.f, 225.f}, sf::Color(255, 154, 127));
            drawText(window, font, "Llegaste al nivel " + std::to_string(level), 24, {371.f, 293.f});
            drawText(window, font, "Pulsa R para volver a intentarlo", 22, {315.f, 342.f});
        }
        else if (victory)
        {
            drawText(window, font, "LO LOGRASTE", 52, {320.f, 225.f}, sf::Color(255, 222, 125));
            drawText(window, font, "Has superado los 10 niveles", 24, {329.f, 293.f});
            drawText(window, font, "Pulsa INTRO para jugar otra vez", 22, {315.f, 342.f});
        }

        if (screen == Screen::MainMenu || screen == Screen::Instructions)
        {
            sf::RectangleShape shade({static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)});
            shade.setFillColor(sf::Color(150, 195, 236, 105));
            window.draw(shade);

            sf::RectangleShape panel({580.f, 470.f});
            panel.setPosition({190.f, 65.f});
            panel.setFillColor(sf::Color(255, 246, 252, 248));
            panel.setOutlineThickness(4.f);
            panel.setOutlineColor(sf::Color(244, 157, 208));
            window.draw(panel);

            const std::array<std::pair<sf::Vector2f, sf::Color>, 8> bubbles{{
                {sf::Vector2f{216.f, 250.f}, sf::Color(159, 220, 246, 210)},
                {sf::Vector2f{728.f, 247.f}, sf::Color(214, 181, 244, 210)},
                {sf::Vector2f{218.f, 418.f}, sf::Color(255, 205, 218, 210)},
                {sf::Vector2f{730.f, 412.f}, sf::Color(255, 222, 153, 220)},
                {sf::Vector2f{247.f, 177.f}, sf::Color(180, 226, 211, 210)},
                {sf::Vector2f{697.f, 177.f}, sf::Color(255, 194, 147, 220)},
                {sf::Vector2f{225.f, 495.f}, sf::Color(214, 181, 244, 210)},
                {sf::Vector2f{718.f, 493.f}, sf::Color(159, 220, 246, 210)},
            }};
            for (const auto& [position, color] : bubbles)
            {
                sf::CircleShape bubble(7.f);
                bubble.setPosition(position);
                bubble.setFillColor(color);
                window.draw(bubble);
                bubble.setRadius(2.5f);
                bubble.setPosition({position.x + 2.f, position.y + 1.f});
                bubble.setFillColor(sf::Color(255, 255, 255, 220));
                window.draw(bubble);
            }

            if (screen == Screen::MainMenu)
            {
                sf::RectangleShape titleRibbon({420.f, 66.f});
                titleRibbon.setPosition({270.f, 72.f});
                titleRibbon.setFillColor(sf::Color(255, 221, 239));
                titleRibbon.setOutlineThickness(2.f);
                titleRibbon.setOutlineColor(sf::Color(255, 255, 255));
                window.draw(titleRibbon);

                drawBalloon(window, {244.f, 306.f}, 0.9f, sf::Color(255, 155, 190));
                drawBalloon(window, {715.f, 314.f}, 0.86f, sf::Color(153, 208, 247));
                drawBalloon(window, {246.f, 378.f}, 0.78f, sf::Color(183, 153, 236));
                drawBalloon(window, {714.f, 386.f}, 0.82f, sf::Color(255, 202, 117));
                drawStar(window, {229.f, 270.f}, 12.f, sf::Color(255, 202, 117));
                drawStar(window, {720.f, 270.f}, 11.f, sf::Color(222, 169, 240));
                drawStar(window, {246.f, 455.f}, 9.f, sf::Color(140, 207, 240));
                drawStar(window, {711.f, 456.f}, 10.f, sf::Color(255, 164, 196));
                drawDinoBuddy(window, {244.f, 126.f}, sf::Color(255, 157, 190),
                              sf::Color(255, 225, 235));
                drawDinoBuddy(window, {716.f, 126.f}, sf::Color(165, 151, 231),
                              sf::Color(232, 224, 255));

                drawCenteredText(window, font, "Carreritas Dino", 48, {480.f, 100.f},
                                 sf::Color(129, 83, 171));
                drawCenteredText(window, font, "Tu aventura empieza aqui", 21, {480.f, 147.f},
                                 sf::Color(89, 113, 151));
                drawCenteredText(window, font, "ELIGE TU DINOSAURIO", 17, {480.f, 188.f},
                                 sf::Color(113, 96, 153));

                for (int dino = 0; dino < static_cast<int>(dinoPalettes.size()); ++dino)
                {
                    const float cardLeft = 267.f + static_cast<float>(dino) * 147.f;
                    const DinoPalette& option = dinoPalettes[static_cast<std::size_t>(dino)];

                    sf::RectangleShape card({132.f, 56.f});
                    card.setPosition({cardLeft, 212.f});
                    card.setFillColor(dino == selectedDino
                                          ? sf::Color(255, 224, 240)
                                          : sf::Color(224, 242, 252));
                    card.setOutlineThickness(dino == selectedDino ? 3.f : 2.f);
                    card.setOutlineColor(dino == selectedDino
                                             ? sf::Color(230, 126, 181)
                                             : sf::Color(143, 196, 221));
                    window.draw(card);

                    sf::CircleShape dinoBody(11.f);
                    dinoBody.setPosition({cardLeft + 11.f, 232.f});
                    dinoBody.setScale({1.25f, 1.f});
                    dinoBody.setFillColor(option.body);
                    window.draw(dinoBody);

                    sf::CircleShape dinoHead(8.f);
                    dinoHead.setPosition({cardLeft + 26.f, 222.f});
                    dinoHead.setFillColor(option.head);
                    window.draw(dinoHead);

                    sf::CircleShape dinoEye(1.5f);
                    dinoEye.setPosition({cardLeft + 37.f, 226.f});
                    dinoEye.setFillColor(sf::Color::White);
                    window.draw(dinoEye);

                    drawCenteredText(window, font, option.name, 17,
                                     {cardLeft + 88.f, 240.f}, sf::Color(87, 86, 128));
                }

                const std::array<std::string, 3> menuLabels{"JUGAR", "INSTRUCCIONES", "SALIR"};
                const std::array<sf::Color, 3> buttonColors{
                    sf::Color(255, 191, 218), sf::Color(202, 180, 239), sf::Color(169, 220, 244)};
                for (int option = 0; option < static_cast<int>(menuLabels.size()); ++option)
                {
                    sf::RectangleShape button({300.f, 52.f});
                    button.setPosition({330.f, 292.f + static_cast<float>(option) * 65.f});
                    sf::Color buttonColor = buttonColors[static_cast<std::size_t>(option)];
                    if (option == selectedMenuOption)
                        buttonColor = {static_cast<std::uint8_t>(std::min(255, buttonColor.r + 18)),
                                       static_cast<std::uint8_t>(std::min(255, buttonColor.g + 18)),
                                       static_cast<std::uint8_t>(std::min(255, buttonColor.b + 18))};
                    button.setFillColor(buttonColor);
                    button.setOutlineThickness(option == selectedMenuOption ? 4.f : 2.f);
                    button.setOutlineColor(option == selectedMenuOption
                                               ? sf::Color(135, 93, 177)
                                               : sf::Color(255, 255, 255));
                    window.draw(button);
                    drawCenteredText(window, font, menuLabels[static_cast<std::size_t>(option)],
                                     22, {480.f, 318.f + static_cast<float>(option) * 65.f},
                                     sf::Color(82, 77, 123));
                }

                drawCenteredText(window, font, "Usa las flechas y Enter, o haz clic para jugar",
                                 14, {480.f, 493.f}, sf::Color(102, 111, 150));
                if (muted)
                    drawCenteredText(window, font, "SONIDO DESACTIVADO (M)", 13, {480.f, 516.f},
                                     sf::Color(182, 93, 130));
            }
            else
            {
                drawCenteredText(window, font, "COMO JUGAR", 40, {480.f, 116.f},
                                 sf::Color(129, 83, 171));
                drawCenteredText(window, font, "ESPACIO o flecha arriba: saltar", 22, {480.f, 190.f},
                                 sf::Color(78, 102, 143));
                drawCenteredText(window, font, "Esquiva los obstaculos", 22, {480.f, 237.f},
                                 sf::Color(78, 102, 143));
                drawCenteredText(window, font, "Completa cada nivel para ganar", 22, {480.f, 284.f},
                                 sf::Color(78, 102, 143));
                drawCenteredText(window, font, "Elige un dino con clic o teclas 1, 2 y 3", 20, {480.f, 331.f},
                                 sf::Color(78, 102, 143));
                drawCenteredText(window, font, "P: pausa     M: sonido", 19, {480.f, 374.f},
                                 sf::Color(78, 102, 143));
                drawCenteredText(window, font, "Escape: volver al menu", 18, {480.f, 407.f},
                                 sf::Color(111, 112, 153));

                sf::RectangleShape backButton({300.f, 52.f});
                backButton.setPosition({330.f, 425.f});
                backButton.setFillColor(sf::Color(185, 222, 245));
                backButton.setOutlineThickness(3.f);
                backButton.setOutlineColor(sf::Color(140, 180, 222));
                window.draw(backButton);
                drawCenteredText(window, font, "VOLVER", 22, {480.f, 451.f},
                                 sf::Color(82, 77, 123));
                drawCenteredText(window, font, "Haz clic o pulsa Escape para volver",
                                 15, {480.f, 494.f}, sf::Color(102, 111, 150));
            }
        }

        if (exitConfirmation)
        {
            sf::RectangleShape shade({static_cast<float>(WindowWidth), static_cast<float>(WindowHeight)});
            shade.setFillColor(sf::Color(62, 54, 91, 150));
            window.draw(shade);

            sf::RectangleShape confirmPanel({500.f, 230.f});
            confirmPanel.setPosition({230.f, 185.f});
            confirmPanel.setFillColor(sf::Color(255, 246, 252, 250));
            confirmPanel.setOutlineThickness(4.f);
            confirmPanel.setOutlineColor(sf::Color(230, 160, 211));
            window.draw(confirmPanel);
            drawCenteredText(window, font, "¿Quieres salir del juego?", 30, {480.f, 237.f},
                             sf::Color(129, 83, 171));
            drawCenteredText(window, font, "Tu partida se cerrara", 18, {480.f, 283.f},
                             sf::Color(89, 113, 151));

            const std::array<std::string, 2> exitLabels{"SI, SALIR", "NO, VOLVER"};
            const std::array<sf::Color, 2> exitColors{
                sf::Color(255, 191, 218), sf::Color(169, 220, 244)};
            for (int option = 0; option < static_cast<int>(exitLabels.size()); ++option)
            {
                sf::RectangleShape button({160.f, 52.f});
                button.setPosition({310.f + static_cast<float>(option) * 180.f, 350.f});
                button.setFillColor(exitColors[static_cast<std::size_t>(option)]);
                button.setOutlineThickness(option == selectedExitOption ? 4.f : 2.f);
                button.setOutlineColor(option == selectedExitOption
                                           ? sf::Color(135, 93, 177)
                                           : sf::Color::White);
                window.draw(button);
                drawCenteredText(window, font, exitLabels[static_cast<std::size_t>(option)],
                                 16, {390.f + static_cast<float>(option) * 180.f, 376.f},
                                 sf::Color(82, 77, 123));
            }
            drawCenteredText(window, font, "Enter: elegir     Escape: volver", 14, {480.f, 425.f},
                             sf::Color(102, 111, 150));
        }

        window.display();
    }
}
