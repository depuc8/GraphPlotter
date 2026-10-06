#pragma once

#include <functional>
#include <string>

// A parsed function of x, e.g. "y = 2x^2 - sin(x)".
//
// Supported syntax:
//   numbers, x, pi, e
//   + - * / ^ (right associative), unary minus, parentheses
//   implicit multiplication: 2x, 3(x+1), (x-1)(x+1), 2sin(x)
//   sin cos tan asin acos atan sinh cosh tanh sqrt abs exp ln log floor ceil
// An optional "y =" or "f(x) =" prefix is ignored.
class Expression {
public:
    // Returns false and fills error() if the text is not a valid expression.
    bool parse(const std::string& text);

    double eval(double x) const { return fn ? fn(x) : 0.0; }
    bool valid() const { return static_cast<bool>(fn); }
    const std::string& error() const { return err; }

private:
    std::function<double(double)> fn;
    std::string err;
};
