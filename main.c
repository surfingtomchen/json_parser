#include <stdio.h>
#include <stdbool.h>

#define UBYTE unsigned char
#define PARSE_ERROR NULL
#define OVERFLOW NULL

typedef enum _JTYPE {
    J_PARSE_ERROR = -1000,

    J_KEY_NOT_FOUND = -100,

    J_STRING = 1,
    J_NUMBER = 2,
    J_OBJ = 3,
    J_ARRAY = 4,
    J_TRUE = 5,
    J_FALSE = 6,
    J_NULL = 7,

} JTYPE;

const int cSOURCE_LENGTH_MAX = 1024 * 100; // 100K bytes
const char cENDING = '\0';

bool isWhiteSpace(UBYTE oneByte) {
    return oneByte == ' ' || oneByte == '\n' || oneByte == '\r' || oneByte == '\t';
};

bool isDigit(UBYTE oneByte) {
    return oneByte >= '0' && oneByte <= '9';
}

bool isE(UBYTE oneByte) {
    return oneByte == 'E' || oneByte == 'e';
}

/**
 * 检查是否是"-"，"+",或者都不是
 * @param oneByte
 * @return
 *        "-": -1
 *        "+": 1
 *        都不是：0
 */
int isNegOrPlus(UBYTE oneByte) {

    if (oneByte == '-') {
        return -1;

    } else if (oneByte == '+') {

        return 1;
    }

    return 0;
}

UBYTE* parseNumber(UBYTE *source, int *length) {

    if ( source[0] != '-' && (source[0]< '0'|| source[0] >'9' ) ) return PARSE_ERROR;

    while()
}

UBYTE *parseTrue(UBYTE *source, int *length) {

    if (source[0] != 't'
        || source[1] != 'r'
        || source[2] != 'u'
        || source[3] != 'e')
        return PARSE_ERROR;

    *length = 4;

    return source;
}

UBYTE *parseFalse(UBYTE *source, int *length) {

    if (source[0] != 'f'
        || source[1] != 'a'
        || source[2] != 'l'
        || source[3] != 's'
        || source[4] != 'e')
        return PARSE_ERROR;

    *length = 5;
    return source;
}

UBYTE *parseNull(UBYTE *source, int *length){

    if (source[0] != 'n'
        || source[1] != 'u'
        || source[2] != 'l'
        || source[3] != 'l')
        return PARSE_ERROR;

    *length = 4;
    return source;
}

/**
 *
 * @param source
 * @param length 包含左右双引号
 * @return
 */
UBYTE *parseString(UBYTE *source, int *length){

    if (source[0] != '"') return PARSE_ERROR;

    int i = 1;

    bool lastIsControl = false;

    while (source[i] != cENDING) {

        UBYTE c = source[i];
        if (c == '\\' && lastIsControl == false) {
            lastIsControl = true;
            i++;
            continue;
        }

        if (c == '"' && lastIsControl == false)break;

        i++;
        lastIsControl = false;
    }

    if (source[i] == cENDING) return OVERFLOW;

    *length = i+1;

    return source;
}

/**
 * 解析key，并且判断是否和targetKey一致, 这里没有判断长度越界，有潜在风险
 * @param source
 * @param targetKey 以0结尾的需要查找的key
 * @param keyLength 用于返回的key长度，包含左右双引号
 * @return 0: PARSE_ERROR, 1: FOUND,  -1: NOT FOUND
 */
int parseKey(UBYTE *source, UBYTE *targetKey, int *keyLength) {

    if (source[0] != '"') return 0;

    int i = 1;

    bool isSame = true;             // 判断是否和targetKey一致
    bool lastIsControl = false;

    while (source[i] != cENDING) {

        UBYTE c = source[i];
        if (c == '"' && lastIsControl == false)break;

        if (isSame) {
            // 这里不会有targetKey越界风险，因为source不会为0
            isSame = source[i] == targetKey[i - 1];
        }

        if (c == '\\' && lastIsControl == false) {
            lastIsControl = true;
            i++;
            continue;
        }

        i++;
        lastIsControl = false;
    }

    if (source[i] == cENDING) return 0;

    *keyLength = i + 1;       // 要包含左右双引号

    return isSame? 1: -1;
}

UBYTE *parseValue(UBYTE *source, int *length, JTYPE *type, int* lengthWithBlanks, UBYTE *targetKey, bool *locatedKeyInObject);

UBYTE *parseObject(UBYTE *source, int *objectLength, UBYTE *targetKey, bool *locatedKeyInObject){

    if ((source[0]) != '{') return PARSE_ERROR;

    int i = 1;
    bool justParsedKey = false;      // 解析完key;
    bool JustParsedValue = false;    // 解析完一对Key，value;
    bool targetKeyFoundInThisLevel = false;

    do {

        UBYTE c = source[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '"') {

            int keyLength = 0;
            JTYPE result;

            if (targetKey != NULL) {

                int result = parseKey(source + i, &keyLength, NULL);
                if (result == 0) return PARSE_ERROR;
                targetKeyFoundInThisLevel = result == 1;

            } else {

                UBYTE* result = parseString(source + i, &keyLength);
                if (result == PARSE_ERROR) return PARSE_ERROR;
            }

            // 指针后移，包括匹配的引号也跳过
            i += keyLength;
            justParsedKey = true;
            continue;
        }

        if (c == ':') {
            if (!justParsedKey) return PARSE_ERROR;

            // 解析本层的对象
            i++;
            int valueLength = 0;
            int lengthWithBlanks = 0;
            JTYPE type = J_PARSE_ERROR;
            UBYTE *result = parseValue(source + i, &valueLength, &type, &lengthWithBlanks, targetKeyFoundInThisLevel? NULL:targetKey, locatedKeyInObject);
            if (result == PARSE_ERROR) return PARSE_ERROR;

            if (targetKeyFoundInThisLevel){

                // key在本层
                *locatedKeyInObject = true;
                *objectLength = valueLength;
                return result;
            }

            if (*locatedKeyInObject){

                // key在下层
                *objectLength = valueLength;
                return result;
            }

            // 继续查找
            JustParsedValue = true;
            justParsedKey = false;
            i += valueLength;
            continue;
        }

        if (c == ',') {

            if (!JustParsedValue) return J_PARSE_ERROR;

            i++;
            JustParsedValue = false;
            continue;
        }

        if (c == '}') {
            break;
        }

    } while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    if(source[i] == cENDING || i>= cSOURCE_LENGTH_MAX ) return OVERFLOW;

    *objectLength = i+1;
    return source;
}

UBYTE *parseArray(UBYTE *source, int *arrayLength, UBYTE *targetKey, bool *locatedKeyInObject){

    int i=0;
    if (source[i] != '[') return PARSE_ERROR;
    i++;

    do{
        UBYTE c = source[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        int valueLength = 0;
        int lengthWithBlanks = 0;
        JTYPE type = J_PARSE_ERROR;
        UBYTE *result = parseValue(source + i, &valueLength, &type, &lengthWithBlanks, targetKey, locatedKeyInObject);
        if (result == PARSE_ERROR) return PARSE_ERROR;

        if (*locatedKeyInObject){

            // key在下层
            *arrayLength = valueLength; // 此时这个长度并不是完整的array，已经不需要完全解析本数组，只需要返回找到的值即可
            return result;
        }

        i++;
        if (source[i] == ','){
            i++;
            continue;
        }

        if (source[i] == ']')break;

    }while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    *arrayLength = i+1;
    return source;
}

UBYTE *parseValue(UBYTE *source, int *length, JTYPE *type, int* lengthWithBlanks, UBYTE *targetKey, bool *locatedKeyInObject) {

    int i=0;
    while(isWhiteSpace(source[i]))i++;

    UBYTE* result = NULL;
    *length = 0;

    switch(source[i]){
        case '{':
            result = parseObject(source+i,length,targetKey,locatedKeyInObject);
            *type = J_OBJ;
            break;
        case '"':
            result = parseString(source+i,length);
            *type = J_STRING;
            break;
        case '[':
            result = parseArray(source+i,length,targetKey,locatedKeyInObject);
            *type = J_ARRAY;
            break;
        case 'f':
            result = parseFalse(source+i, length);
            *type = J_FALSE;
            break;
        case 't':
            result = parseTrue(source+i,length);
            *type = J_TRUE;
            break;
        case 'n':
            result = parseNull(source+i,length);
            *type = J_NULL;
            break;
        default:
            result = parseNumber(source+i,length);
            *type = J_NUMBER;
            break;
    }

    if (result == NULL) {
        *type = J_PARSE_ERROR;
    }

    i+=length;

    while(isWhiteSpace(source[i]))i++;

    *lengthWithBlanks=i;

    return result;
}


JTYPE macroKeyValue(UBYTE *source, UBYTE *targetKey, void *foundValue) {

    int i = 0;
    UBYTE c;
    do {
        c = source[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '{') {

            return parseObject(source + i, targetKey, foundValue);

        } else {

            return J_PARSE_ERROR;
        }

    } while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    return J_PARSE_ERROR;
}

JTYPE macroArray(UBYTE *source, int index, void *foundValue) {

    int i = 0;
    UBYTE c;

    do {

        c = source[i];
        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '[') {
            return parseArray(source + i, index, foundValue);
        } else {
            return J_PARSE_ERROR;
        }

    } while (c != cENDING && i < cSOURCE_LENGTH_MAX);

    return J_PARSE_ERROR;
}

int main() {

    UBYTE buf[100] = "{\"x\":\"1\"}";
    void *valueFound = NULL;
    JTYPE result = macroKeyValue(buf, "x", &valueFound);
    printf("Result is %d", result);
    return 0;
}
