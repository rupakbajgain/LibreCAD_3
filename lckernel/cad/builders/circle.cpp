#include "circle.h"
#include <cad/primitive/circle.h>
#include <cad/math/lcmath.h>

const lc::geo::Coordinate& lc::builder::CircleBuilder::center() const {
    return _center;
}

lc::builder::CircleBuilder* lc::builder::CircleBuilder::setCenter(const lc::geo::Coordinate& center) {
    _center = center;
    return this;
}

double lc::builder::CircleBuilder::radius() const {
    return _radius;
}

lc::builder::CircleBuilder* lc::builder::CircleBuilder::setRadius(double radius) {
    _radius = radius;
    return this;
}

/* Algebraic solution from Problem of Apollonius - https://en.wikipedia.org/wiki/Problem_of_Apollonius
*  Calculation code snippet from rosettacode.org - https://rosettacode.org/wiki/Problem_of_Apollonius
*/
bool lc::builder::CircleBuilder::threeTanConstructor(lc::entity::CADEntity_CSPtr circ0, lc::entity::CADEntity_CSPtr circ1, lc::entity::CADEntity_CSPtr circ2, float s1, float s2, float s3)
{
    lc::entity::Circle_CSPtr circle0 = std::dynamic_pointer_cast<const lc::entity::Circle>(circ0);
    lc::entity::Circle_CSPtr circle1 = std::dynamic_pointer_cast<const lc::entity::Circle>(circ1);
    lc::entity::Circle_CSPtr circle2 = std::dynamic_pointer_cast<const lc::entity::Circle>(circ2);

    // check if selected entities are in fact circles, if not return false
    if (circle0 == nullptr || circle1 == nullptr || circle2 == nullptr) {
        return false;
    }

    double x1 = circle0->getCenter().x();
    double y1 = circle0->getCenter().y();
    double x2 = circle1->getCenter().x();
    double y2 = circle1->getCenter().y();
    double x3 = circle2->getCenter().x();
    double y3 = circle2->getCenter().y();
    double r1 = circle0->getRadius();
    double r2 = circle1->getRadius();
    double r3 = circle2->getRadius();

    double v11 = 2 * x2 - 2 * x1;
    double v12 = 2 * y2 - 2 * y1;
    double v13 = x1 * x1 - x2 * x2 + y1 * y1 - y2 * y2 - r1 * r1 + r2 * r2;
    double v14 = 2 * s2 * r2 - 2 * s1 * r1;

    double v21 = 2 * x3 - 2 * x2;
    double v22 = 2 * y3 - 2 * y2;
    double v23 = x2 * x2 - x3 * x3 + y2 * y2 - y3 * y3 - r2 * r2 + r3 * r3;
    double v24 = 2 * s3 * r3 - 2 * s2 * r2;

    double w12 = v12 / v11;
    double w13 = v13 / v11;
    double w14 = v14 / v11;

    double w22 = v22 / v21 - w12;
    double w23 = v23 / v21 - w13;
    double w24 = v24 / v21 - w14;

    double P = -w23 / w22;
    double Q = w24 / w22;
    double M = -w12 * P - w13;
    double N = w14 - w12 * Q;

    double a = N * N + Q * Q - 1;
    double b = 2 * M * N - 2 * N * x1 + 2 * P * Q - 2 * Q * y1 + 2 * s1 * r1;
    double c = x1 * x1 + M * M - 2 * M * x1 + P * P + y1 * y1 - 2 * P * y1 - r1 * r1;

    std::vector<double> coffs;
    coffs.push_back(b / a);
    coffs.push_back(c / a);
    std::vector<double> sol = lc::maths::Math::quadraticSolver(coffs);

    double r = 0;

    // taking only positive roots
    for (double d : sol) {
        if (d > 0) {
            r = d;
        }
    }

    double xs = M + N * r;
    double ys = P + Q * r;

    _center = lc::geo::Coordinate(xs, ys);
    _radius = r;

    return true;
}

lc::entity::Circle_CSPtr lc::builder::CircleBuilder::build() {
    return entity::Circle_CSPtr(new entity::Circle(*this));
}
