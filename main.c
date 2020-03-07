#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define UBYTE unsigned char
#define PARSE_ERROR NULL
#define OVERFLOW NULL

typedef enum {
    J_PARSE_ERROR = -1000,

    J_NOT_FOUND = 0,

    J_STRING = 1,
    J_NUMBER = 2,
    J_OBJ = 3,
    J_ARRAY = 4,
    J_TRUE = 5,
    J_FALSE = 6,
    J_NULL = 7,

} ObjectType;

typedef struct {
    UBYTE       *objectKey;
    ObjectType  objectType;
    bool        keyFoundInObject;
} Search;

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

UBYTE* parseNumber(UBYTE *source, int *length) {

    int i=0;
    UBYTE c = source[i];
    if ( c != '-' && (c < '0'|| c >'9' ) ) return PARSE_ERROR;

    if (c == '-')i++;

    c=source[i];
    bool hasDotAlready = false;

    if (c == '0'){
        if (source[i+1] != '.'){
            *length = i+1;
            return source;
        }else {
            i++;
        }
    }

    do{
        c = source[i];
        if (c == '.' && hasDotAlready) return PARSE_ERROR;
        if (c == '.') hasDotAlready = true;
        i++;
    }while ( (isDigit(source[i]) || source[i] == '.') && i <cSOURCE_LENGTH_MAX && source[i] != cENDING);

    if (i>=cSOURCE_LENGTH_MAX || source[i] == cENDING) return OVERFLOW;

    *length = i;
    return source;
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
int parseKey(const UBYTE *source, const UBYTE *targetKey, int *keyLength) {

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

    *keyLength = i + 1;

    isSame = isSame && targetKey[i-1] == cENDING;

    return isSame? 1: -1;
}

UBYTE *parseValue(UBYTE *source, int *length, int* lengthWithBlanks, Search* search);

UBYTE *parseObject(UBYTE *source, int *objectLength, Search *search){

    if ((source[0]) != '{') { search->objectType = J_PARSE_ERROR; return PARSE_ERROR;}

    int i = 1;
    bool justParsedKey = false;      // 解析完key;
    bool JustParsedValue = false;    // 解析完一对Key，value;
    bool targetKeyFoundInThisLevel = false;

    UBYTE c;
    do {
        c = source[i];
        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '"') {

            int keyLength = 0;
            if (search->objectKey != NULL) {

                int result = parseKey(source + i, search->objectKey, &keyLength);
                if (result == 0) { search->objectType = J_PARSE_ERROR; return PARSE_ERROR;}
                targetKeyFoundInThisLevel = result == 1;
                if (targetKeyFoundInThisLevel)search->objectKey = NULL;

            } else {

                UBYTE* result = parseString(source + i, &keyLength);
                if (result == PARSE_ERROR) { search->objectType = J_PARSE_ERROR; return PARSE_ERROR;}
            }

            // 指针后移，包括匹配的引号也跳过
            i += keyLength;
            justParsedKey = true;
            continue;
        }

        if (c == ':') {
            if (!justParsedKey) { search->objectType=J_PARSE_ERROR; return PARSE_ERROR;}

            // 解析本层的对象
            i++;
            int valueLength = 0;
            int lengthWithBlanks = 0;



            UBYTE *result = parseValue(source + i, &valueLength, &lengthWithBlanks,search);
            if (result == PARSE_ERROR) { search->objectType = J_PARSE_ERROR; return PARSE_ERROR;}

            if (targetKeyFoundInThisLevel){

                // key在本层
                search->keyFoundInObject = true;
                *objectLength = valueLength;
                return result;
            }

            if (search->keyFoundInObject){

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

            if (!JustParsedValue) { search->objectType = J_PARSE_ERROR; return PARSE_ERROR;}

            i++;
            JustParsedValue = false;
            continue;
        }

        if (c == '}') {
            break;
        }

    } while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    if(source[i] == cENDING || i>= cSOURCE_LENGTH_MAX ) { search->objectType = J_PARSE_ERROR; return OVERFLOW;}

    *objectLength = i+1;

    search->objectType = J_NOT_FOUND;
    return source;
}

UBYTE *parseArray(UBYTE *source, int *arrayLength, Search *search){

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
        UBYTE *result = parseValue(source + i, &valueLength, &lengthWithBlanks, search);
        if (result == PARSE_ERROR) return PARSE_ERROR;

        if (search->keyFoundInObject){

            // key在下层已经找到
            *arrayLength = valueLength; // 此时这个长度并不是完整的array，已经不需要完全解析本数组，只需要返回找到的值即可
            return result;
        }

        i += lengthWithBlanks;

        if (source[i] == ','){
            i++;
            continue;
        }

        if (source[i] == ']')break;

    }while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    *arrayLength = i+1;
    return source;
}

UBYTE *parseValue(UBYTE *source, int *length, int* lengthWithBlanks, Search *search) {

    int i=0;
    while(isWhiteSpace(source[i]))i++;

    UBYTE* result = NULL;
    *length = 0;

    switch(source[i]){
        case '{':
        {
            bool before = search->keyFoundInObject;
            result = parseObject(source+i,length,search);
            if (before == search->keyFoundInObject){
                search->objectType = J_OBJ;
            }
            break;
        }
        case '"':
            result = parseString(source+i,length);
            search->objectType = J_STRING;
            break;
        case '[':
        {
            bool before = search->keyFoundInObject;
            result = parseArray(source + i, length,search);
            if (before == search->keyFoundInObject){
                search->objectType = J_ARRAY;
            }
            break;
        }
        case 'f':
            result = parseFalse(source+i, length);
            search->objectType = J_FALSE;
            break;
        case 't':
            result = parseTrue(source+i,length);
            search->objectType = J_TRUE;
            break;
        case 'n':
            result = parseNull(source+i,length);
            search->objectType = J_NULL;
            break;
        default:
            result = parseNumber(source+i,length);
            search->objectType = J_NUMBER;
            break;
    }

    if (result == NULL) {
        search->objectType = J_PARSE_ERROR;
    }

    i+=*length;
    while(isWhiteSpace(source[i]))i++;

    *lengthWithBlanks=i;
    return result;
}

void *getResultByType(UBYTE *pByte, ObjectType type, int length){

    switch (type){
        case J_PARSE_ERROR:
            return PARSE_ERROR;

        case J_NUMBER: {
            char *doubleStr = (char *)malloc(sizeof(char)*length +1);
            memcpy(doubleStr,pByte,length+1);
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
        case J_OBJ:{
            UBYTE *value = (UBYTE *)malloc(sizeof(UBYTE)*length+1);
            memcpy(value,pByte,length);
            value[length] = cENDING;

            return value;
        }

        case J_STRING:{
            UBYTE *value = (UBYTE *)malloc(sizeof(UBYTE)*length-1);
            memcpy(value,pByte+1,length-2);
            value[length-2] = cENDING;

            return value;
        }

        default: // not found
            return NULL;
    }
}

void* macroKeyValue(UBYTE *source, Search *search) {

    int i = 0;
    UBYTE c;
    do {
        c = source[i];

        if (isWhiteSpace(c)) {
            i++;
            continue;
        }

        if (c == '{') {
            int length = 0;
            UBYTE *start = parseObject(source + i, &length,search);
            return getResultByType(start, search->objectType, length);

        } else {
            search->objectType = J_PARSE_ERROR;
            return PARSE_ERROR;
        }

    } while (source[i] != cENDING && i < cSOURCE_LENGTH_MAX);

    search->objectType = J_PARSE_ERROR;
    return PARSE_ERROR;
}

void* macroArray(UBYTE *source, int index, Search *search) {

    int i = 0;
    UBYTE c;

    while (isWhiteSpace(source[i]) && i < cSOURCE_LENGTH_MAX)i++;
    if(i>=cSOURCE_LENGTH_MAX) {
        search->objectType = J_PARSE_ERROR;
        return OVERFLOW;
    }

    if (source[i] != '['){
        search->objectType = J_PARSE_ERROR;
        return PARSE_ERROR;
    }

    do {
        c = source[++i];
        if (isWhiteSpace(c))continue;

        if (c==']')break;

        int length = 0;
        int lengthWithBlank = 0;
        UBYTE *valueStart = parseValue(source+i,&length,&lengthWithBlank, search);

        if (valueStart == PARSE_ERROR) {
            search->objectType = J_PARSE_ERROR;
            return PARSE_ERROR;
        }

        if(index-- == 0 ){

            // 找到当前的value
            return getResultByType(valueStart,search->objectType,length);

        }else {

            i+=lengthWithBlank;

            if(source[i] == ',') {
                i++; // just skip ','
            }else if(source[i]== ']'){
                i--; // 如果间隔符号刚好是']'的情况
            }else {
                search->objectType = J_PARSE_ERROR;
                return PARSE_ERROR;
            }
        }

    } while (c != cENDING && i < cSOURCE_LENGTH_MAX);

    search->objectType = J_PARSE_ERROR;
    return PARSE_ERROR;
}

void test(char *src, char *key, char *expected){

    Search search = {(UBYTE*)key,J_NOT_FOUND,false};
    void *result = macroKeyValue(src, &search);

    char buf[2048];
    switch(search.objectType){
        case J_NOT_FOUND:
            sprintf(buf,"not found...");
            break;
        case J_NUMBER:
            sprintf(buf,"number is %f",*(double*)result);
            break;
        case J_PARSE_ERROR:
            sprintf(buf,"parse error");
            break;
        case J_STRING:
            sprintf(buf,"string is %s",(char*)result);
            break;
        case J_ARRAY:
            sprintf(buf,"array is %s",(char*)result);
            break;
        case J_OBJ:
            sprintf(buf,"obj is %s",(char*)result);
            break;
        case J_TRUE:
            sprintf(buf,"value is true");
            break;
        case J_FALSE:
            sprintf(buf,"value is false");
            break;
        default: //case J_NULL:
            sprintf(buf,"value is null");
            break;
    }

    if (strcmp(buf,expected) == 0){
        printf("test passed!\n");
    }else {
        printf("test failed, expected: %s, actual: %s\n",expected,buf);
    }
}

void test2(char *src, int index, char* expected){

    Search search = {NULL,J_NOT_FOUND,false};
    void *result = macroArray(src,index,&search);

    char buf[2048];
    switch(search.objectType){
        case J_NOT_FOUND:
            sprintf(buf,"not found...");
            break;
        case J_NUMBER:
            sprintf(buf,"number is %f",*(double*)result);
            break;
        case J_PARSE_ERROR:
            sprintf(buf,"parse error");
            break;
        case J_STRING:
            sprintf(buf,"string is %s",(char*)result);
            break;
        case J_ARRAY:
            sprintf(buf,"array is %s",(char*)result);
            break;
        case J_OBJ:
            sprintf(buf,"obj is %s",(char*)result);
            break;
        case J_TRUE:
            sprintf(buf,"value is true");
            break;
        case J_FALSE:
            sprintf(buf,"value is false");
            break;
        default: //case J_NULL:
            sprintf(buf,"value is null");
            break;
    }

    if (strcmp(buf,expected) == 0){
        printf("test passed!\n");
    }else {
        printf("test failed, expected: %s, actual: %s\n",expected,buf);
    }
}


int main() {

    test("{\"x\":\"1\"}","x","string is 1");
    test("    {   \"x22\"   :  123  }","x22", "number is 123.000000");
    test("    {   \"x22\"   :  12890000  }","x222", "not found...");
    test("    {   \"x22\"   :  [ 12.99, 0.989, 99.9, \"\",{} ]  }","x22", "array is [ 12.99, 0.989, 99.9, \"\",{} ]");
    test("    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ]  }","1","value is true");
    test("    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ],\"3\": null  }","3","value is null");
    test("    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\",{} ],\"3\": { "
         "\"user\":{},"
         "\"data\":\"x12345\""
         "}","data","string is x12345");
    test("    {  \"1\": true,  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : \"59\" } ],\"3\": null  }","data","string is 59");
    test("    {  \"我0\": true,  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : 59.980 } ],\"3\": null  }","我0","value is true");
    test("    {  \"0\": \"1233\",  \"x22\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": \"我的\"  }","3","string is 我的");
    test("    {  \"0\": \"12{33\",  \"x2}2\"   :  [ 12, 99, 99, \"\", { \"data\" : 59 } ],\"3\": {}  }","3","obj is {}");
    test("    {     }   ","1","not found...");
    test(" {   \"x\":  12}","","not found...");
    test(" {   \"x\":  12}","xx","not found...");
    test(" {   \"xxx\":  12}","xx","not found...");
    test(" {   ","xx","parse error");

    test2("{\"user\":\"1234\"}",0,"parse error");
    test2("{\"user\":\"1234\"}",1,"parse error");
    test2("   [  \"user\"   ,    \"1234\"   ]    ",1,"string is 1234");
    test2("   [  \"user\"   ,    \"1234\"   }    ",1,"string is 1234");
    test2("   [  \"user\"   ]   \"1234\"   }    ",1,"parse error");
    test2("   [  \"user\"   ]   \"1234\"       ",0,"string is user");

    return 0;
}