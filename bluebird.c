#include <stdio.h>
#include <string.h>

#include "1-parse.h"
#include "2-build.h"
#include "4-determinize.h"
#include "5-check-for-ambiguity.h"
#include "6a-generate.h"
#include "6b-interpret.h"

#define DEBUG_OUTPUT 1

// TODO:
// - ambiguity checking
// - don't output unreachable nfa states
//  - this probably means filtering out intermediate states from the epsilon
//    closure
// - fancy interpreter output
// - json interpreter output
// - clean up memory leaks
// - get rid of RULE_LOOKUP?
// - check if all the input is properly validated
// - error messages that include line/column/visuals
// - perf optimization?

// ASIDE:
// - write a blog post or something about how the old parser worked?

static void output_stdout(struct generator *g, const char *string, size_t len);

int main(int argc, char *argv[])
{
    const char *string =
    //"a = b* [ '(' b* ')' ] | b [ '(' b | c ')' ]  b = 'x'  c = 'x'";
    //"a = [ '(' b ')' ] | [ '(' c ')' ]  b = 'x'  c = 'x'*";
    //"a = b | c  b = [ '(' 'x' ')' ]  c = [ '(' 'x'* ')' ]";
    "a = [ '(' b* ')' ] | [ '(' c? ')' ]  b = 'x'?  c = 'y'?";
    //"a = b? b?  b = 'x'";
    //"a = [ '(' a ')' ]";
    //"a = 'x'*";
    //"a = '' x*  x = 'x'";
    //"a = b*  b = 'x'";
    //"a = 'a' 'b' 'c'";
    //"a = b  b = 'x'";
    //"a = b b  b = 'x'?";
    //"a = identifier | number";
    //"a = b@x [ '(' [ a@x ] ')' ]  b = a";
    //"a = identifier ('+' identifier)*";
    //"a = identifier `ident`  infix flat $ '+' `plus`";
    //"a = b*  b = identifier";
    //"a = b@x b = 'foo'";
    //"a = [ '(' [ a ] ')' ] | 'x'";
    /*
    "expr = [ '(' [ expr ] ')' ] `parens`"
     "identifier `ident`"
     "number `literal`"
     "infix left $ '*' `times` '/' `divided-by`"
     "infix left $ '+' `plus` '-' `minus`";
    // */
    //"expr = identifier `ident`  postfix $ '*' `zero-or-more`  infix flat $ '|' `choice`  infix flat $ test `test`  test = [ '(' [ expr@nested ] ')' ]";
    //"expr = identifier `ident`  number `number`  infix flat $ '+' `plus`";
    //"grammar = rule*   rule = identifier '=' body   body = expr | (expr ':' identifier)+ operators*   operators = '.operators' fixity operator+   operator = expr ':' identifier   fixity =    'postfix' `postfix`    'prefix' `prefix`    'infix' assoc `infix`   assoc =    'flat' `flat`    'left' `left`    'right' `right`    'nonassoc' `nonassoc`    expr =     identifier ('@' identifier@rename)? `ident`     string `literal`     [ '(' [ expr ] ')' ] `parens`     [ '[' [ identifier@left expr? identifier@right ] ']' ] `bracketed`    postfix $     '*' `zero-or-more`     '+' `one-or-more`     '?' `optional`    infix flat $     '' `concatenation`    infix flat $     '|' `choice`";
    //"grammar = rule*   rule = identifier '=' body   body = expr | (expr ':' identifier)+ operators*   operators = '.operators' fixity operator+   operator = expr ':' identifier   fixity =    'postfix' : postfix    'prefix' : prefix    'infix' assoc : infix   assoc =    'flat' : flat    'left' : left    'right' : right    'nonassoc' : nonassoc    expr =     identifier ('@' identifier@rename)? : ident     string : literal     [ '(' expr ')' ] : parens     [ '[' string@left expr? string@right ']' ] : bracketed   .operators postfix     '*' : zero-or-more     '+' : one-or-more     '?' : optional    .operators infix flat     '' : concatenation    .operators infix flat     '|' : choice";
/*
    "grammar = rule* "
    "rule = identifier '=' body "
    "body = expr | (expr ':' identifier)+ operators* "
    "operators = '.operators' fixity operator+ "
    "operator = expr ':' identifier "
    "fixity =    'postfix' : postfix-op    'prefix' : prefix-op    'infix' assoc : infix-op "
    "assoc =    'flat' : flat-op    'left' : left-op    'right' : right-op    'nonassoc' : nonassoc-op "
    "expr = "
    "identifier ('@' identifier@rename)? : ident "
    "string : literal "
    "[ '(' expr ')' ] : parens "
    "[ '[' string@begin-token expr? string@end-token ']' ] : bracketed "
    ".operators postfix "
    "'*' : zero-or-more "
    "'+' : one-or-more "
    "'?' : optional "
    ".operators infix flat "
    "'' : concatenation "
    ".operators infix flat "
    "'|' : choice ";
 //*/
    //"a = [ '(' a ')' ] | [ '(' b ')' ] b = [ '(' a ')' ] | [ '(' b ')' ]";
    //"a = [s[a]e] | [s[b]e] b = [s[a]e] | [s[b]e]";
    //"a = b b b = c c c = d d d = e e e = f f f = g g g = h h h = i i i = j j j = k k k = l l l = m m m = n n n = o o o = p p p = q q q = r r r = s s s = t t t = u u u = 'v'";// v v = w w w = x x x = y y y = z";
    struct bluebird_tree *tree = bluebird_tree_create_from_string(string);
#if DEBUG_OUTPUT
    bluebird_tree_print(tree);
#endif

    struct grammar grammar = {0};
    build(&grammar, tree);

#if DEBUG_OUTPUT
    for (uint32_t i = 0; i < grammar.number_of_rules; ++i) {
        if (grammar.rules[i].is_token) {
            printf("%u: token '%.*s'\n", i, (int)grammar.rules[i].name_length,
             grammar.rules[i].name);
            continue;
        }
        printf("%u: rule '%.*s'\n", i, (int)grammar.rules[i].name_length,
         grammar.rules[i].name);
        if (grammar.rules[i].number_of_choices == 0)
            automaton_print(&grammar.rules[i].automaton);
        else {
            for (uint32_t j = 0; j < grammar.rules[i].number_of_choices; ++j) {
                printf("choice %u: '%.*s'\n", j,
                 (int)grammar.rules[i].choices[j].name_length,
                 grammar.rules[i].choices[j].name);
                automaton_print(&grammar.rules[i].choices[j].automaton);
            }
            for (uint32_t j = 0; j < grammar.rules[i].number_of_operators; ++j){
                printf("operator %u (prec %d): '%.*s'\n", j,
                 grammar.rules[i].operators[j].precedence,
                 (int)grammar.rules[i].operators[j].name_length,
                 grammar.rules[i].operators[j].name);
                automaton_print(&grammar.rules[i].operators[j].automaton);
            }
        }
        if (grammar.rules[i].number_of_keyword_tokens) {
            printf("keywords:\n");
            for (uint32_t j = 0; j < grammar.rules[i].number_of_keyword_tokens; ++j) {
                printf("\t%.*s (type %d): %x\n",
                 (int)grammar.rules[i].keyword_tokens[j].length,
                 grammar.rules[i].keyword_tokens[j].string,
                 grammar.rules[i].keyword_tokens[j].type,
                 grammar.rules[i].keyword_tokens[j].symbol);
            }
        }
        if (grammar.rules[i].number_of_slots) {
            printf("slots:\n");
            for (uint32_t j = 0; j < grammar.rules[i].number_of_slots; ++j) {
                printf("\t%.*s: %x -> %u\n",
                 (int)grammar.rules[i].slots[j].name_length,
                 grammar.rules[i].slots[j].name,
                 grammar.rules[i].slots[j].symbol,
                 grammar.rules[i].slots[j].rule_index);
            }
        }
        if (grammar.rules[i].number_of_brackets) {
            printf("brackets:\n");
            for (uint32_t j = 0; j < grammar.rules[i].number_of_brackets; ++j) {
                printf("%x -> [ %x %x ]\n", grammar.rules[i].brackets[j].symbol,
                 grammar.rules[i].brackets[j].start_symbol,
                 grammar.rules[i].brackets[j].end_symbol);
                automaton_print(&grammar.rules[i].brackets[j].automaton);
            }
        }
    }
    printf("---\n");
#endif
    struct combined_grammar combined = {0};
    combine(&combined, &grammar);
#if DEBUG_OUTPUT
    automaton_print(&combined.automaton);
    automaton_print(&combined.bracket_automaton);
    for (uint32_t i = 0; i < combined.number_of_tokens; ++i) {
        printf("token %x: %.*s\n", i, (int)combined.tokens[i].length,
         combined.tokens[i].string);
    }
#endif

#if DEBUG_OUTPUT
    printf("------\n");
#endif
    struct bracket_transitions bracket_transitions = {0};
    determinize_bracket_transitions(&bracket_transitions, &combined);
#if DEBUG_OUTPUT
    printf("------\n");
#endif

    struct deterministic_grammar deterministic = {0};
    determinize(&combined, &deterministic, &bracket_transitions);
#if DEBUG_OUTPUT
    automaton_print(&deterministic.automaton);
    automaton_print(&deterministic.bracket_automaton);
#endif

    printf("- ambiguity start -\n");
    check_for_ambiguity(&combined, &bracket_transitions);
    printf("- ambiguity end -\n");

    //const char *text_to_parse = "x x x x x x";
    const char *text_to_parse = "x";
    //const char *text_to_parse = "a + (b / c) + c";
    //const char *text_to_parse = "a | b | c | d | e";
    //const char *text_to_parse = "a + b + c + d + 3";
    //const char *text_to_parse = "a + b + c * (e + f + g) + h * 7 + a0 + a1 + a2";
    //const char *text_to_parse = "a + (b - c) * d / e + f + g * h";
    //const char *text_to_parse = "(((x)))";
    //const char *text_to_parse = "test = a | b*  a = identifier  b = number";
    //const char *text_to_parse = "test = [ 'x' (a@b | a1 'a2' a3) 'y' ] | (c | b)* : eee  .operators infix left  p : p  .operators prefix pre : pre";
    //const char *text_to_parse = "q + (x + y) + z + ((d + ((w))) + r) + k";
    //const char *text_to_parse = "a = (b)";
#if DEBUG_OUTPUT
    interpret(&grammar, &combined, &bracket_transitions, &deterministic, text_to_parse);
#endif

#if !DEBUG_OUTPUT
    struct generator generator = {
        .output = output_stdout,
        .grammar = &grammar,
        .combined = &combined,
        .deterministic = &deterministic,
        .transitions = &bracket_transitions,
    };
    generate(&generator);
#endif

    /*
    const char *tok;
#define EVALUATE_MACROS_AND_STRINGIFY(...) #__VA_ARGS__
#define TOKENIZE_BODY(...) tok = EVALUATE_MACROS_AND_STRINGIFY(__VA_ARGS__);
#define READ_KEYWORD_TOKEN(...) 0
#include "x-tokenize.h"
    printf("%s\n", tok);
*/

    bluebird_tree_destroy(tree);
    return 0;
}

static void output_stdout(struct generator *g, const char *string, size_t len)
{
    fwrite(string, len, 1, stdout);
}
