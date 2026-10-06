#include "Parser.hpp"

#include <cctype>
#include <cmath>
#include <map>
#include <stdexcept>

using namespace std;

namespace {

using Fn = function<double(double)>;

// Recursive descent parser. Each rule returns a closure that evaluates
// that part of the expression for a given x.
//
//   expr    := term (('+' | '-') term)*
//   term    := unary (('*' | '/') unary | <implicit *> power)*
//   unary   := ('-' | '+') unary | power
//   power   := primary ('^' unary)?
//   primary := number | name | name '(' expr ')' | '(' expr ')'
class Parser {
public:
    explicit Parser(const string& s) : src(s) {}

    Fn parse(){
        Fn f = expr();
        skipSpaces();
        if(pos < src.size())
            fail(string("unexpected '") + src[pos] + "'");
        return f;
    }

private:
    const string& src;
    size_t pos = 0;

    [[noreturn]] void fail(const string& msg){
        throw runtime_error(msg + " at column " + to_string(pos + 1));
    }

    void skipSpaces(){
        while(pos < src.size() && isspace(static_cast<unsigned char>(src[pos]))) pos++;
    }

    char peek(){
        skipSpaces();
        return pos < src.size() ? src[pos] : '\0';
    }

    bool accept(char c){
        if(peek() == c){ pos++; return true; }
        return false;
    }

    Fn expr(){
        Fn left = term();
        for(;;){
            if(accept('+')){
                Fn right = term();
                left = [left, right](double x){ return left(x) + right(x); };
            } else if(accept('-')){
                Fn right = term();
                left = [left, right](double x){ return left(x) - right(x); };
            } else {
                return left;
            }
        }
    }

    Fn term(){
        Fn left = unary();
        for(;;){
            char c = peek();
            if(accept('*')){
                Fn right = unary();
                left = [left, right](double x){ return left(x) * right(x); };
            } else if(accept('/')){
                Fn right = unary();
                left = [left, right](double x){ return left(x) / right(x); };
            } else if(isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '('){
                // Implicit multiplication: "2x", "3(x+1)", "x sin(x)"
                Fn right = power();
                left = [left, right](double x){ return left(x) * right(x); };
            } else {
                return left;
            }
        }
    }

    Fn unary(){
        if(accept('-')){
            Fn operand = unary();
            return [operand](double x){ return -operand(x); };
        }
        if(accept('+')) return unary();
        return power();
    }

    Fn power(){
        Fn base = primary();
        if(accept('^')){
            Fn exponent = unary();  // right associative, allows 2^-x
            return [base, exponent](double x){ return pow(base(x), exponent(x)); };
        }
        return base;
    }

    Fn primary(){
        char c = peek();

        if(accept('(')){
            Fn inner = expr();
            if(!accept(')')) fail("missing ')'");
            return inner;
        }

        if(isdigit(static_cast<unsigned char>(c)) || c == '.'){
            size_t used = 0;
            double value;
            try {
                value = stod(src.substr(pos), &used);
            } catch(const exception&){
                fail("bad number");
            }
            pos += used;
            return [value](double){ return value; };
        }

        if(isalpha(static_cast<unsigned char>(c))){
            size_t start = pos;
            while(pos < src.size() && isalpha(static_cast<unsigned char>(src[pos]))) pos++;
            string name = src.substr(start, pos - start);
            return named(name, start);
        }

        if(c == '\0') fail("unexpected end of expression");
        fail(string("unexpected '") + c + "'");
    }

    Fn named(const string& name, size_t start){
        if(name == "x") return [](double x){ return x; };
        if(name == "pi") return [](double){ return M_PI; };
        if(name == "e") return [](double){ return M_E; };

        static const map<string, double (*)(double)> functions = {
            {"sin", ::sin},   {"cos", ::cos},   {"tan", ::tan},
            {"asin", ::asin}, {"acos", ::acos}, {"atan", ::atan},
            {"sinh", ::sinh}, {"cosh", ::cosh}, {"tanh", ::tanh},
            {"sqrt", ::sqrt}, {"abs", ::fabs},  {"exp", ::exp},
            {"ln", ::log},    {"log", ::log10},
            {"floor", ::floor}, {"ceil", ::ceil},
        };

        auto it = functions.find(name);
        if(it == functions.end()){
            pos = start;
            fail("unknown name '" + name + "'");
        }
        if(!accept('(')) fail("expected '(' after " + name);
        Fn arg = expr();
        if(!accept(')')) fail("missing ')'");

        double (*f)(double) = it->second;
        return [f, arg](double x){ return f(arg(x)); };
    }
};

} // namespace

bool Expression::parse(const string& text){
    // Drop an optional "y =" / "f(x) =" prefix.
    size_t eq = text.find('=');
    string body = (eq == string::npos) ? text : text.substr(eq + 1);

    try {
        Parser parser(body);
        Fn parsed = parser.parse();
        fn = parsed;
        err.clear();
        return true;
    } catch(const exception& ex){
        err = ex.what();
        return false;
    }
}
