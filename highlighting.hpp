//
// Created by Alex Gallon on 15/07/2020.
//

#ifndef GTASM_HIGHLIGHTING_HPP
#define GTASM_HIGHLIGHTING_HPP

#include "miss2/constructs.hpp"

struct RGBColor {
    uint8_t r, g, b;

    RGBColor() = default;

    RGBColor(uint8_t red, uint8_t green, uint8_t blue) {
        r = red;
        g = green;
        b = blue;
    }

    std::string colorString() {
        return "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    }

    std::string toString() {
        return std::string("rgb(") + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")";
    }

    void premultiply(float a) {
        r = float(r) * a;
        g = float(g) * a;
        b = float(b) * a;
    }
};

static std::string white = RGBColor{255, 255, 255}.colorString();
static std::string green = RGBColor{100, 255, 100}.colorString();
static std::string blue = RGBColor{100, 100, 255}.colorString();
static std::string red = RGBColor{255, 100, 100}.colorString();
static std::string codeColor = RGBColor{200, 255, 255}.colorString();
static std::string gray = RGBColor{100, 100, 100}.colorString();
static std::string varColor =  RGBColor{255, 200, 200}.colorString();
static std::string pink = RGBColor{255, 150, 200}.colorString();
static std::string orange = RGBColor{255, 150, 0}.colorString();
static std::string blueGreen = RGBColor{0, 220, 200}.colorString();
static std::string callColor = RGBColor{255, 255, 100}.colorString();

static std::string asComment(string_ref s) {
    return gray + s;
}

static std::string asErr(string_ref s) {
    return red + s;
}

static std::string asNormal(string_ref s) {
    return white + s;
}

#endif //GTASM_HIGHLIGHTING_HPP
