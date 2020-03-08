//
// Created by Tom CHEN on 2020-03-06.
//

#ifndef UNTITLED_MAIN_H
#define UNTITLED_MAIN_H

#include <stdbool.h>

#define UBYTE unsigned char

/* Json Object type */
typedef enum {
    J_PARSE_ERROR           = -1000,
    J_PATTERN_WRONG_FORMAT  = -1001,

    J_NOT_FOUND             = 0,

    J_INT                   = 1,
    J_FLOAT                 = 2,

    J_STRING                = 10,
    J_OBJ                   = 20,
    J_ARRAY                 = 30,
    J_TRUE                  = 40,
    J_FALSE                 = 50,
    J_NULL                  = 60
} ValueType;

/* Search Options
 * 0: Normal, if not found in top level, then return not found
 * 1: Recursive, if not found in current level, will keep founding in sub level
 */
typedef enum {
    S_NORMAL,
    S_RECURSIVE
} SearchOptions;

typedef struct {
    UBYTE           *pattern;
    ValueType       valueType;
    bool            keyFoundInObject;
    SearchOptions   options;
} Search;

/** public interface **/

/**
 * 使用路径字符串进行查找
 * 举例说明：
 *  .key.nextKey1.nextLevelKey2     -->代表根节点下的key所对应的obj中nextKey1所对应obj的nextLevelKey2对应的value
 *  [3][2]                          -->代表根节点下的数据格式是数组，数组第4个元素对应的值还是个数组，取第3个value（下标从0开始）
 *  [1].data[2]                     -->代表根节点下的数据格式是数组，取数组第2个元素中key名称为data的数据为数组的第3个value
 *  .user[2].anotherKey             -->代表根节点数据格式为对象，其中key为user的数组中第三个元素的key为anotherKey的value
 *
 * @param input
 * @param search
 * @return 查询结果的内容，需要手动释放指针
 */
void *marcoPathSearch(const UBYTE *input, Search *search);

/**
 * 提供Key的search方法，支持当前层和Recursive查找
 * @param input
 * @param search
 * @return 查询结果的内容, 需要手动释放指针
 */
void *macroKeyValueSearch(const UBYTE *input, Search *search);

/**
 * 提供Index的search方法，当且仅当Json字符串是数组的情况下查找
 * @param input
 * @param index 以0为开始的下标
 * @param search
 * @return 查询结果的内容，需要手动释放指针
 */
void *macroArrayIndexSearch(const UBYTE *input,int index, Search *search);

#endif //UNTITLED_MAIN_H
