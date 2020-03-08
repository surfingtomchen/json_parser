//
// Created by Tom CHEN on 2020-03-06.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "main.h"

#define PARSE_ERROR NULL
#define NOT_FOUND NULL
#define OVER_FLOW NULL
#define PATTERN_WRONG_FORMAT NULL

const int cSOURCE_LENGTH_MAX = 1024 * 100;      // 100K bytes max length for json string
const char cENDING = '\0';                      // ending char for string or the input json string

bool isWhiteSpace(UBYTE oneByte) {
    return oneByte == ' ' || oneByte == '\n' || oneByte == '\r' || oneByte == '\t';
};

bool isDigit(UBYTE oneByte) {
    return oneByte >= '0' && oneByte <= '9';
}

/**
 * @param input
 * @param length
 * @return 0: PARSE_ERROR,  1:  Int,    2: Float
 */
int parseNumber(const UBYTE *input, int *length) {

    int i = 0;
    UBYTE c = input[i];
    if (c != '-' && (c < '0' || c > '9')) return (int) PARSE_ERROR;

    if (c == '-')i++;

    c = input[i];
    bool hasDotAlready = false;

    if (c == '0') {
        if (input[i + 1] != '.') {
            *length = i + 1;
            return (int) J_INT;
        } else {
            i++;
        }
    }

    do {
        c = input[i];
        if (c == '.' && hasDotAlready) return (int) PARSE_ERROR;
        if (c == '.') hasDotAlready = true;
        i++;
    } while ((isDigit(input[i]) || input[i] == '.') && i < cSOURCE_LENGTH_MAX && input[i] != cENDING);

    if (i >= cSOURCE_LENGTH_MAX || input[i] == cENDING) return (int) OVER_FLOW;

    *length = i;
    return (int) (hasDotAlready ? J_FLOAT : J_INT);
}

UBYTE *parseTrue(const UBYTE *input, int *length) {

    if (input[0] != 't'
        || input[1] != 'r'
        || input[2] != 'u'
        || input[3] != 'e')
        return PARSE_ERROR;

    *length = 4;
    return input;
}

UBYTE *parseFalse(const UBYTE *input, int *length) {

    if (input[0] != 'f'
        || input[1] != 'a'
        || input[2] != 'l'
        || input[3] != 's'
        || input[4] != 'e')
        return PARSE_ERROR;

    *length = 5;
    return input;
}

UBYTE *parseNull(const UBYTE *input, int *length) {

    if (input[0] != 'n'
        || input[1] != 'u'
        || input[2] != 'l'
        || input[3] != 'l')
        return PARSE_ERROR;

    *length = 4;
    return input;
}

UBYTE *parseString(const UBYTE *input, int *length) {

    if (input[0] != '"') return PARSE_ERROR;

    int i = 1;

    bool lastIsControl = false;

    while (input[i] != cENDING) {

        UBYTE c = input[i];
        if (c == '\\' && lastIsControl == false) {
            lastIsControl = true;
            i++;
            continue;
        }

        if (c == '"' && lastIsControl == false)break;

        i++;
        lastIsControl = false;
    }

    if (input[i] == cENDING) return OVER_FLOW;

    *length = i + 1;
    return input;
}

/**
 * 解析key，并且判断是否和targetKey一致, 这里没有判断长度越界，有潜在风险
 * @param input
 * @param targetKey 以0结尾的需要查找的key
 * @param keyLength 用于返回的key长度，包含左右双引号
 * @return 0: PARSE_ERROR, 1: FOUND,  -1: NOT FOUND
 */
int parseKey(const UBYTE *input, const UBYTE *targetKey, int *keyLength) {

    if (input[0] != '"') return (int) PARSE_ERROR;

    int i = 1;

    bool isSame = true;             // 判断是否和targetKey一致
    bool lastIsControl = false;

    while (input[i] != cENDING) {

        UBYTE c = input[i];
        if (c == '"' && lastIsControl == false)break;

        if (isSame) {
            // 这里不会有targetKey越界风险，因为source不会为0
            isSame = input[i] == targetKey[i - 1];
        }

        if (c == '\\' && lastIsControl == false) {
            lastIsControl = true;
            i++;
            continue;
        }

        i++;
        lastIsControl = false;
    }

    if (input[i] == cENDING) return (int) PARSE_ERROR;

    *keyLength = i + 1;

    isSame = isSame && targetKey[i - 1] == cENDING;

    return isSame ? 1 : -1;
}

UBYTE *parseValue(const UBYTE *input, int *length, int *lengthWithBlanks, Search *search, ValueType *valueType);

UBYTE *parseObject(const UBYTE *input, int *objectLength, Search *search) {

    if ((input[0]) != '{') {
        search->valueType = J_PARSE_ERROR;
        return PARSE_ERROR;
    }

    int i = 1;
    bool justParsedKey = false;      // 解析完key;
    bool JustParsedValue = false;    // 解析完一对Key，value;
    bool targetKeyFoundInThisLevel = false;

    UBYTE c;
    do {
        c = input[i];
        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '"') {

            int keyLength = 0;
            if (search->pattern != NULL) {

                int result = parseKey(input + i, search->pattern, &keyLength);
                if (result == 0) {
                    search->valueType = J_PARSE_ERROR;
                    return PARSE_ERROR;
                }
                targetKeyFoundInThisLevel = result == 1;
                if (targetKeyFoundInThisLevel)search->pattern = NULL;

            } else {

                UBYTE *result = parseString(input + i, &keyLength);
                if (result == PARSE_ERROR) {
                    search->valueType = J_PARSE_ERROR;
                    return PARSE_ERROR;
                }
            }

            // 指针后移，包括匹配的引号也跳过
            i += keyLength;
            justParsedKey = true;
            continue;
        }

        if (c == ':') {
            if (!justParsedKey) {
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }

            // 解析本层的对象
            i++;
            int valueLength = 0;
            int lengthWithBlanks = 0;


            UBYTE *result;
            ValueType valueType;
            if (search->options == S_RECURSIVE) {
                result = parseValue(input + i, &valueLength, &lengthWithBlanks, search, &valueType);
            } else {
                Search searchNextLevel = {NULL, search->valueType, search->keyFoundInObject, S_NORMAL};
                result = parseValue(input + i, &valueLength, &lengthWithBlanks, &searchNextLevel, &valueType);
            }

            if (result == PARSE_ERROR) {
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }

            if (targetKeyFoundInThisLevel) {

                // key在本层
                search->keyFoundInObject = true;
                search->valueType = valueType;
                *objectLength = valueLength;
                return result;
            }

            if (search->keyFoundInObject) {
                // key在下层已经找到
                *objectLength = valueLength;
                return result;
            }

            // 继续查找
            JustParsedValue = true;
            justParsedKey = false;
            i += lengthWithBlanks;
            continue;
        }

        if (c == ',') {

            if (!JustParsedValue) {
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }

            i++;
            JustParsedValue = false;
            continue;
        }

        if (c == '}') {
            break;
        }

    } while (input[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    if (input[i] == cENDING || i >= cSOURCE_LENGTH_MAX) {
        search->valueType = J_PARSE_ERROR;
        return OVER_FLOW;
    }

    *objectLength = i + 1;

    search->valueType = J_NOT_FOUND;
    return input;
}

UBYTE *parseArray(const UBYTE *input, int *arrayLength, Search *search) {

    int i = 0;
    if (input[i] != '[') return PARSE_ERROR;
    i++;

    do {
        UBYTE c = input[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        int valueLength = 0;
        int lengthWithBlanks = 0;
        ValueType valueType;

        UBYTE *result;
        if (search->options == S_NORMAL) {
            Search normalSearch = {NULL, search->valueType, search->keyFoundInObject, S_NORMAL};
            result = parseValue(input + i, &valueLength, &lengthWithBlanks, &normalSearch, &valueType);
        } else {
            result = parseValue(input + i, &valueLength, &lengthWithBlanks, search, &valueType);
        }

        if (result == PARSE_ERROR) return PARSE_ERROR;

        if (search->keyFoundInObject) {
            // key在下层已经找到
            *arrayLength = valueLength; // 此时这个长度并不是完整的array，已经不需要完全解析本数组，只需要返回找到的值即可
            return result;
        }

        i += lengthWithBlanks;

        if (input[i] == ',') {
            i++;
            continue;
        }

        if (input[i] == ']')break;

    } while (input[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    *arrayLength = i + 1;
    return input;
}

UBYTE *parseValue(const UBYTE *input, int *length, int *lengthWithBlanks, Search *search, ValueType *valueType) {

    int i = 0;
    while (isWhiteSpace(input[i]))i++;

    UBYTE *result = NULL;
    *length = 0;

    switch (input[i]) {
        case '{': {
            result = parseObject(input + i, length, search);
            *valueType = J_OBJ;
            break;
        }
        case '"':
            result = parseString(input + i, length);
            *valueType = J_STRING;
            break;
        case '[': {
            result = parseArray(input + i, length, search);
            *valueType = J_ARRAY;
            break;
        }
        case 'f':
            result = parseFalse(input + i, length);
            *valueType = J_FALSE;
            break;
        case 't':
            result = parseTrue(input + i, length);
            *valueType = J_TRUE;
            break;
        case 'n':
            result = parseNull(input + i, length);
            *valueType = J_NULL;
            break;
        default: {
            int r = parseNumber(input + i, length);
            if (r == (int) PARSE_ERROR) {
                result = PARSE_ERROR;
            } else {
                result = input + i;
                *valueType = r == 1 ? J_INT : J_FLOAT;
            }
            break;
        }
    }

    if (result == NULL) {
        search->valueType = J_PARSE_ERROR;
    }

    i += *length;
    while (isWhiteSpace(input[i]))i++;

    *lengthWithBlanks = i;
    return result;
}

void *getActualValueByType(const UBYTE *input, ValueType type, int length) {

    switch (type) {
        case J_PARSE_ERROR:
            return PARSE_ERROR;

        case J_INT: {
            char *intStr = (char *) malloc(sizeof(char) * length + 1);
            memcpy(intStr, input, length + 1);
            intStr[length] = cENDING;

            int *value = malloc(sizeof(int));
            *value = round(strtod(intStr, NULL));
            return value;
        }
        case J_FLOAT: {
            char *doubleStr = (char *) malloc(sizeof(char) * length + 1);
            memcpy(doubleStr, input, length + 1);
            doubleStr[length] = cENDING;

            double *value = malloc(sizeof(double));
            *value = strtod(doubleStr, NULL);
            return value;
        }

        case J_NULL:
            return NULL;

        case J_TRUE: {
            bool *value = malloc(sizeof(bool));
            *value = true;
            return value;
        }

        case J_FALSE: {
            bool *value = malloc(sizeof(bool));
            *value = false;
            return value;
        }

        case J_ARRAY:
        case J_OBJ: {
            UBYTE *value = (UBYTE *) malloc(sizeof(UBYTE) * length + 1);
            memcpy(value, input, length);
            value[length] = cENDING;
            return value;
        }

        case J_STRING: {
            UBYTE *value = (UBYTE *) malloc(sizeof(UBYTE) * length - 1);
            memcpy(value, input + 1, length - 2);
            value[length - 2] = cENDING;
            return value;
        }

        default: // not found
            return NULL;
    }
}

void *macroKeyValueSearch(const UBYTE *input, Search *search) {

    int length = 0;
    int lengthWithBlank = 0;
    ValueType valueType;
    UBYTE *valueBegin = parseValue(input, &length, &lengthWithBlank, search, &valueType);
    if (valueBegin == PARSE_ERROR) {
        search->valueType = J_PARSE_ERROR;
        return PARSE_ERROR;
    }

    return getActualValueByType(valueBegin, search->valueType, length);
}

void *macroArrayIndexSearch(const UBYTE *input, int index, Search *search) {

    int i = 0;
    UBYTE c;
    search->options = S_NORMAL;

    while (isWhiteSpace(input[i]) && i < cSOURCE_LENGTH_MAX)i++;
    if (i >= cSOURCE_LENGTH_MAX) {
        search->valueType = J_PARSE_ERROR;
        return OVER_FLOW;
    }

    if (input[i] != '[') {
        search->valueType = J_PARSE_ERROR;
        return PARSE_ERROR;
    }

    do {
        c = input[++i];
        if (isWhiteSpace(c))continue;

        if (c == ']')break;

        int length = 0;
        int lengthWithBlank = 0;
        ValueType valueType;
        UBYTE *valueStart = parseValue(input + i, &length, &lengthWithBlank, search, &valueType);

        if (valueStart == PARSE_ERROR) {
            search->valueType = J_PARSE_ERROR;
            return PARSE_ERROR;
        }

        if (index-- == 0) {

            // 找到当前的value
            search->valueType = valueType;
            return getActualValueByType(valueStart, search->valueType, length);

        } else {

            i += lengthWithBlank;

            if (input[i] == ',') {
                // just skip
            } else if (input[i] == ']') {
                i--; // 如果间隔符号刚好是']'的情况
            } else {
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }
        }

    } while (c != cENDING && i < cSOURCE_LENGTH_MAX);

    search->valueType = J_PARSE_ERROR;
    return PARSE_ERROR;
}

#define PATH_SEPARATE    '.'
void *marcoPathSearch(const UBYTE *input, Search *search) {

    UBYTE *pattern = search->pattern;
    search->options = S_NORMAL;
    search->keyFoundInObject = false;

    if (pattern == NULL || (*pattern != PATH_SEPARATE && *pattern != '[')) {
        search->valueType = J_PATTERN_WRONG_FORMAT;
        return PATTERN_WRONG_FORMAT;
    }

    void *source = input;

    while (*pattern != cENDING) {

        if (*pattern == PATH_SEPARATE) {
            // search in object
            pattern++;

            UBYTE *keyStart = pattern, *tempKey;
            int keyLength = 0;

            while (*pattern != PATH_SEPARATE && *pattern != '[' && *pattern != ']' && *pattern != cENDING) {
                keyLength++;
                pattern++;
            }
            tempKey = (UBYTE *) malloc(sizeof(UBYTE *) * keyLength + 1);
            memcpy(tempKey, keyStart, keyLength + 1);
            tempKey[keyLength] = cENDING;

            // ready to search
            Search keySearch = {tempKey, J_NOT_FOUND, false, S_NORMAL};
            int length, lengthWithBlanks;
            ValueType valueType;

            void *result = parseValue((UBYTE *) source, &length, &lengthWithBlanks, &keySearch, &valueType);
            free(tempKey);

            // not found the value and parse error
            if (result == PARSE_ERROR) {
                if (source != input) free(source);
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }

            // not found the value
            if (keySearch.valueType == J_NOT_FOUND) {
                if (source != input) free(source);
                search->valueType = J_NOT_FOUND;
                return NOT_FOUND;
            }

            // found the value
            search->valueType = keySearch.valueType;

            // do some cleaning to prevent the memory leak
            if (source != input) free(source);

            // recurse search if path not empty
            source = getActualValueByType(result, keySearch.valueType, length);
            continue;
        }

        if (*pattern == '[') {
            // search in array
            pattern++;

            UBYTE *numberStart = pattern, *tempIntStr;
            int keyLength = 0;

            while (*pattern != ']' && *pattern != cENDING) {
                keyLength++;
                pattern++;
            }
            if (*pattern == cENDING) {
                if (source != input) free(source);
                search->valueType = J_PATTERN_WRONG_FORMAT;
                return PATTERN_WRONG_FORMAT;
            }
            tempIntStr = (UBYTE *) malloc(sizeof(UBYTE *) * keyLength + 1);
            memcpy(tempIntStr, numberStart, keyLength + 1);
            tempIntStr[keyLength] = cENDING;

            char *err;
            int index = round(strtod(tempIntStr, &err));
            if (index < 0 || *err != 0) {
                if (source != input) free(source);
                search->valueType = J_PATTERN_WRONG_FORMAT;
                return PATTERN_WRONG_FORMAT;
            }

            free(tempIntStr);
            Search tempSearch = {NULL, J_NOT_FOUND, false};
            void *result = macroArrayIndexSearch(source, index, &tempSearch);
            if (result == PARSE_ERROR) {
                if (source != input) free(source);
                search->valueType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }

            // not found the value
            if (tempSearch.valueType == J_NOT_FOUND) {
                if (source != input) free(source);
                search->valueType = J_NOT_FOUND;
                return NOT_FOUND;
            }

            // found the value
            search->valueType = tempSearch.valueType;

            // do some cleaning to prevent the memory leak
            if (source != input) free(source);

            // recurse search if path not empty
            source = result;
            pattern++;
            continue;
        }

        search->valueType = J_PATTERN_WRONG_FORMAT;
        return PATTERN_WRONG_FORMAT;
    }

    return source;
}

/**********************************************************************************************************************/

/* 从这里以下是测试代码 */
void printTestResult(char *name, char *result, char *expected, ValueType valueType) {

    char buf[2048];
    switch (valueType) {
        case J_NOT_FOUND:
            sprintf(buf, "not found...");
            break;
        case J_PATTERN_WRONG_FORMAT:
            sprintf(buf, "wrong pattern format");
            break;
        case J_FLOAT:
            sprintf(buf, "number is %f", *(double *) result);
            break;
        case J_INT:
            sprintf(buf, "number is %d", *(int *) result);
            break;
        case J_PARSE_ERROR:
            sprintf(buf, "parse error");
            break;
        case J_STRING:
            sprintf(buf, "string is %s", (char *) result);
            break;
        case J_ARRAY:
            sprintf(buf, "array is %s", (char *) result);
            break;
        case J_OBJ:
            sprintf(buf, "obj is %s", (char *) result);
            break;
        case J_TRUE:
            sprintf(buf, "value is true");
            break;
        case J_FALSE:
            sprintf(buf, "value is false");
            break;
        default: //case J_NULL:
            sprintf(buf, "value is null");
            break;
    }

    if (strcmp(buf, expected) == 0) {
        printf("%s, test passed!\n", name);
    } else {
        printf("%s, test failed, expected: [%s], actual: [%s]\n", name, expected, buf);
    }
}

void test(char *name, char *input, char *key, char *expected, bool isRecursive) {

    Search search = {(UBYTE *) key, J_NOT_FOUND, false, isRecursive ? S_RECURSIVE : S_NORMAL};
    void *result = macroKeyValueSearch((UBYTE *) input, &search);

    printTestResult(name, result, expected, search.valueType);
}

void test2(char *name, char *input, int index, char *expected) {

    Search search = {NULL, J_NOT_FOUND, false, S_NORMAL};
    void *result = macroArrayIndexSearch((UBYTE *) input, index, &search);

    printTestResult(name, result, expected, search.valueType);
}

void test3(char *name, char *input, char *pattern, char *expected) {

    Search search = {pattern,J_NOT_FOUND,false,S_NORMAL};
    void *result = marcoPathSearch((UBYTE *) input, &search);

    printTestResult(name, result, expected, search.valueType);
}

int main() {

    test("1", "{\"x\":\"1\"}", "x", "string is 1", true);
    test("2", "    {   \"x22\"   :  123  }", "x22", "number is 123", true);
    test("3", "    {   \"x22\"   :  12890000  }", "x222", "not found...", true);
    test("4", "    {   \"x22\"   :  [ 12.99, 0.989, 99.9, \"\",{} ]  }", "x22",
         "array is [ 12.99, 0.989, 99.9, \"\",{} ]", true);
    test("5", "    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ]  }", "1", "value is true", true);
    test("6", "    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ],\"3\": null  }", "3", "value is null", true);
    test("7", "    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ],\"3\": { "
              "\"user\":{},"
              "\"data\":\"x12345\""
              "}", "data", "string is x12345", true);
    test("8", "    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : \"59\" } ],\"3\": null  }", "data",
         "string is 59", true);
    test("9", "    {  \"我0\": true,  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : 59.980 } ],\"3\": null  }", "我0",
         "value is true", true);
    test("10", "    {  \"0\": \"1233\",  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": \"我的\"  }", "3",
         "string is 我的", true);
    test("11", "    {  \"0\": \"12{33\",  \"x2}2\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": {}  }", "3",
         "obj is {}", true);
    test("12", "    {     }   ", "1", "not found...", true);
    test("13", " {   \"x\":  12}", "", "not found...", true);
    test("14", " {   \"x\":  12}", "xx", "not found...", true);
    test("15", " {   \"xxx\":  12}", "xx", "not found...", true);
    test("16", " {   ", "xx", "parse error", true);

    test("17", "    {  \"0\": \"12{33\",  \"x2}2\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": {}  }", "data",
         "not found...", false);
    test("18", "    {  \"0\": \"12{33\",  \"x2}2\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": {}  }", "3",
         "obj is {}", false);


    test2("19", "{\"user\":\"1234\"}", 0, "parse error");
    test2("20", "{\"user\":\"1234\"}", 1, "parse error");
    test2("21", "   [  \"user\"   ,    0.1234   ]    ", 1, "number is 0.123400");
    test2("22", "   [  \"user\"   ,    \"1234\"   }    ", 1, "string is 1234");
    test2("23", "   [  \"user\"   ]   \"1234\"   }    ", 1, "parse error");
    test2("24", "   [  0   ]   99      ", 0, "number is 0");

    UBYTE *json = "{\n"
                  "  \"code\": 1000,\n"
                  "  \"msg\": null,\n"
                  "  \"data\": {\n"
                  "    \"user\": {\n"
                  "      \"userId\": 100001,\n"
                  "      \"address\": \"安徽 黄山\",\n"
                  "      \"age\": 20,\n"
                  "      \"avatar\": \"http://www.qiniu.com/img/we.png\",\n"
                  "      \"avatarList\": [\n"
                  "               {\n"
                  "                    \"avatarId\": \"JIHJSKJHKHH987JG\",\n"
                  "                    \"url\": \"http://www.qiniu.com/img/we.png\",\n"
                  "                    \"status\": 1\n"
                  "                },\n"
                  "                 {\n"
                  "                    \"avatarId\": \"JIHJSKJHKHH987JG\",\n"
                  "                    \"url\": \"http://www.qiniu.com/img/we.png\",\n"
                  "                    \"status\": 100\n"
                  "                }\n"
                  "      ],\n"
                  "      \"nick\": \"ss9df11\",\n"
                  "      \"callRatio\": 90,\n"
                  "      \"price\": 0,\n"
                  "      \"selfIntro\": \"我是一只小小小小鸟\",\n"
                  "       \"selfAudioIntro\" : \"http://wwwww\",\n"
                  "       \"second\": 34,\n"
                  "      \"setting\": 0,\n"
                  "      \"account\": 0,\n"
                  "      \"followCount\":10,\n"
                  "      \"fansCount\":2,\n"
                  "      \"talkTime\":456,\n"
                  "      \"tags\":\"发放,温柔,儿童\",\n"
                  "      \"gifts\": [\n"
                  "       {\n"
                  "         \"name\":\"草莓\",\n"
                  "         \"image\":\"http://ww.qiniu.com/img/gift1.png\",\n"
                  "         \"price\":400,\n"
                  "         \"amount\":\"4\",\n"
                  "       },\n"
                  "       {\n"
                  "         \"name\":\"香蕉\",\n"
                  "         \"image\":\"http://ww.qiniu.com/img/gift2.png\",\n"
                  "         \"price\":1400,\n"
                  "         \"amount\":\"40\"\n"
                  "       }],\n"
                  "       \"loginDuration\" : \"1\",\n"
                  "       \"recentVisitors\":[\n"
                  "       {\n"
                  "          \"userId\" : 123456,\n"
                  "          \"nick\" : \"昵称昵称\",\n"
                  "          \"avatar\" : \"http://staticnova.ruoogle.com/avatar/user24566.png\"\n"
                  "       },\n"
                  "       {\n"
                  "          \"userId\" : 123457,\n"
                  "          \"nicks\" : \"东方不败\",\n"
                  "          \"avatar\" : \"http://staticnova.ruoogle.com/avatar/user24567.png\"\n"
                  "       }\n"
                  "       ]\n"
                  "    }\n"
                  "  }\n"
                  "}";
    test("25", json, "nicks", "string is 东方不败", true);

    test3("26", json, ".data.user.userId", "number is 100001");
    test3("27", json, ".data]39[", "wrong pattern format");
    test3("28", json, "[.9.9]", "wrong pattern format");
    test3("29", json, "[.9.]", "wrong pattern format");
    test3("30", json, ".data.user.avatarList[1].status", "number is 100");
    json = "[ [ 1, 2 ,3  ]  ,[  2  ,  3,   4  ],[3,4,  [  4 , 5 , 4   ]  ]  ]";
    test2("31",json,2,"array is [3,4,  [  4 , 5 , 4   ]  ]");
    test3("32", json, "[2][2][1]", "number is 5");

    return 0;
}