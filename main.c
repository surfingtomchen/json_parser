#include <stdio.h>
#include <stdbool.h>

#define UBYTE unsigned char

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

//typedef struct ParseResult {
//
//    JTYPE result;
//    union value{
//        unsigned char byteValue,
//        unsigned short doubleByteValue,
//        unsigned int 32bitValue,
//        unsigned long long 64bitValue,
//        double  doubleValue,
//    } value;
//};

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

    if ( source[0] != '-' && (source[0]< '0'|| source[0] >'9' ) ) return NULL;

    while()
}

UBYTE *parseTrue(UBYTE *source, int *length) {

    if (source[0] != 't'
        || source[1] != 'r'
        || source[2] != 'u'
        || source[3] != 'e')
        return NULL;

    *length = 4;

    return source;
}

UBYTE *parseFalse(UBYTE *source, int *length) {

    if (source[0] != 'f'
        || source[1] != 'a'
        || source[2] != 'l'
        || source[3] != 's'
        || source[4] != 'e')
        return NULL;

    *length = 5;
    return source;
}

UBYTE *parseNull(UBYTE *source, int *length){

    if (source[0] != 'n'
        || source[1] != 'u'
        || source[2] != 'l'
        || source[3] != 'l')
        return NULL;

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

    if (source[0] != '"') return NULL;

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

    if (source[i] == cENDING) return NULL;

    *length = i+1;

    return source;
}

UBYTE *parseObject(UBYTE *source, int *length){

}

UBYTE *parseArray(UBYTE *source, int *length){

}

UBYTE *parseValue(UBYTE *source, int *length, JTYPE *type, int* lengthWithBlanks) {

    int i=0;
    while(isWhiteSpace(source[i]))i++;

    UBYTE* result = NULL;
    *length = 0;

    switch(source[i]){
        case '{':
            result = parseObject(source+i,length);
            *type = J_OBJ;
            break;
        case '"':
            result = parseString(source+i,length);
            *type = J_STRING;
            break;
        case '[':
            result = parseString(source+i,length);
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

/**
 * 解析key，并且判断是否和targetKey一致, 这里没有判断长度越界，有潜在风险
 * @param source
 * @param targetKey 以0结尾的需要查找的key
 * @param keyLength 用于返回的key长度，包含左右双引号
 * @return 是否相等，如果keyLength 为0，表示解析key失败
 */
JTYPE parseKey(UBYTE *source, UBYTE *targetKey, int *keyLength) {

    if (source[0] != '"') return J_PARSE_ERROR;

    int i = 1;

    bool isSame = true;             // 判断是否和tarkey一致
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

    if (source[i] == cENDING) return J_PARSE_ERROR;

    *keyLength = i + 1;       // 要包含左右双引号

    if (isSame) return J_STRING;    // 这里实际上只要不等于 J_KEY_NOT_FOUND的值即可

    return J_KEY_NOT_FOUND;
}


JTYPE parseValueRecursive(UBYTE *source, UBYTE *targetkey, int *valueLength, void *targetValue){

    int i = 0;
    while (source[i] != cENDING && isWhiteSpace(source[i])) {
        i++;
    }
    if (source[i] == cENDING) return J_PARSE_ERROR;

    UBYTE c = source[i];
    JTYPE result;

    int length = 0;
    switch (c) {
        case '{':
            result = parseObject(source, targetKey, targetValue, &length);
            break;
        case '[':
            result = parseArray(source, targetKey, targetValue, &length);
            break;
        case '"':
            result = parseString(source, targetValue, &length);
            break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            result = parseNumber(source, targetValue, &length);
            break;
        case 't':
            result = parseTrue(source, (bool *) targetValue);
            length = 4;
            break;
        case 'f':
            result = parseFalse(source, (bool *) targetValue);
            length = 5;
            break;
        case 'n':
            result = parseNull(source);
            length = 4;
            break;
    }

    if (result == J_PARSE_ERROR) return J_PARSE_ERROR;

    i += length;

    while (source[i] != cENDING && isWhiteSpace(source[i])) {
        i++;
    }
    if (source[i] == cENDING) return J_PARSE_ERROR;

    *valueLength = i;
    return result;
}

void* parseValue(UBYTE *source, JTYPE *type) {

    int i = 0;
    while (source[i] != cENDING && isWhiteSpace(source[i])) {
        i++;
    }
    if (source[i] == cENDING) return J_PARSE_ERROR;

    UBYTE c = source[i];
    JTYPE result;

    int length = 0;
    switch (c) {
        case '{':

            break;
        case '[':
            result = parseArray(source, targetKey, targetValue, &length);
            break;
        case '"':
            result = parseString(source, targetValue, &length);
            break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            result = parseNumber(source, targetValue, &length);
            break;
        case 't':
            result = parseTrue(source, (bool *) targetValue);
            length = 4;
            break;
        case 'f':
            result = parseFalse(source, (bool *) targetValue);
            length = 5;
            break;
        case 'n':
            result = parseNull(source);
            length = 4;
            break;
    }

    if (result == J_PARSE_ERROR) return J_PARSE_ERROR;

    i += length;

    while (source[i] != cENDING && isWhiteSpace(source[i])) {
        i++;
    }
    if (source[i] == cENDING) return J_PARSE_ERROR;

    *valueLength = i;
    return result;
}

/**
 * 解析对象
 * @param source
 * @param targetkey 需要在对象内recursive 寻找的key
 * @param targetValue   如果寻找到key，对应的value
 * @param objectLength  object的长度，含两边{}
 * @return
 */
JTYPE parseObject(UBYTE *source, UBYTE *targetkey, void *targetValue, int *objectLength) {

    if ((source[0]) != '{') return J_PARSE_ERROR;

    int i = 1;
    bool aKeyFound = false;      // 解析到key的开始;
    bool aValueFound = false;    // 解析完一对Key，value;
    bool targetKeyFound = false; // 找到targetKey

    do {

        UBYTE c = source[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '"') {

            aKeyFound = true;
            int keyLength = 0;
            JTYPE result;

            if (targetKeyFound) {
                result = parseKey(source + i, &keyLength, NULL);
            } else {
                result = parseKey(source + i, &keyLength, targetkey);
            }

            if (result == J_PARSE_ERROR) return J_PARSE_ERROR;

            // 判断是否找到target key
            if(!targetKeyFound)targetKeyFound = !(result == J_KEY_NOT_FOUND);

            // 指针后移，包括匹配的引号也跳过
            i += keyLength;
            continue;
        }

        if (c == ':') {
            if (!aKeyFound) return J_PARSE_ERROR;

            // 解析本层的对象
            i++;
            int valueLength = 0;
            JTYPE result = parseValue(source + i, value, &valueLength, targetKeyFound? NULL:targetkey);
            if (result == J_PARSE_ERROR) return J_PARSE_ERROR;




            if(!targetKeyFound)targetKeyFound = !(result == J_KEY_NOT_FOUND);
            if (targetKeyFound) return result;  // 递归返回

            aValueFound = true;
            aKeyFound = false;
            i += valueLength;
            continue;
        }

        if (c == ',') {

            if (!aValueFound) return J_PARSE_ERROR;

            i++;
            aValueFound = false;
            continue;
        }

        if (c == '}') {
            break;
        }

        return J_PARSE_ERROR;

    } while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    if (!targetKeyFound) return J_KEY_NOT_FOUND;

    return J_PARSE_ERROR;
}

JTYPE parseArray(UBYTE *source, int index, void *value) {

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
