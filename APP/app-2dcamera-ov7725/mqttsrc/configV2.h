#ifndef _CONFIG_V2_H_
#define _CONFIG_V2_H_

#include <stdbool.h>

#define  MAX_VALUE  64 /* 定义section,key,value字符串最大长度 */
// printf("File = %s\nLine = %d\nFunc=%s\nDate=%s\nTime=%s\n", __FILE__, __LINE__, __FUNCTION__, __DATE__, __TIME__);
#define  PRINT_ERRMSG(STR) fprintf(stderr,"line:%d,msg:%s,eMsg:%s\n", __LINE__, STR, strerror(errno))

typedef struct _option {
  char    key[MAX_VALUE];   /* 对应键 */
  char    value[MAX_VALUE]; /* 对应值 */
  struct  _option *next;    /* 链表连接标识 */
}Option;

typedef struct _data {
  char    section[MAX_VALUE]; /* 保存section值          */
  Option  *option;            /* option链表头           */
  struct  _data *next;        /* 链表连接标识           */
}Data;

typedef struct {
  char    comment;              /* 表示注释的符号    */
  char    separator;            /* 表示分隔符        */
  char    re_string[MAX_VALUE]; /* 返回值字符串的值  */
  int     re_int;               /* 返回int的值       */
  bool    re_bool;              /* 返回bool的值      */
  double  re_double ;           /* 返回double类型    */
  Data    *data;                /* 保存数据的头      */
}Config;

/**
* 判断字符串是否为空
* 为空返回true,不为空返回false
**/
bool str_empty(const char *string);

/**
* 向链表添加section,key,value
* 如果添加时不存在section则新增一个
* 如果对应section的key不存在则新增一个
* 如果section已存在则不会重复创建
* 如果对应section的key已存在则只会覆盖key的值
**/
bool cnf_add_option(Config *cnf, const char *section, const char *key, const char *value);

/**
* 按照参数去除字符串左右空白
**/
char *trim_string(char *string,int mode);

/**
* 传递配置文件路径
* 参数有文件路径,注释字符,分隔符
* 返回Config结构体
**/
Config *cnf_read_config(const char *filename, char comment, char separator);

/**
* 获取指定类型的值
* 根据不同类型会赋值给对应值
* 本方法需要注意,int和double的转换,不满足就是0
*     需要自己写代码时判断好
**/
bool cnf_get_value(Config *cnf, const char *section, const char *key);

/**
* 判断section是否存在
* 不存在返回空指针
* 存在则返回包含那个section的Data指针
**/
Data *cnf_has_section(Config *cnf, const char *section);

/**
* 判断指定option是否存在
* 不存在返回空指针
* 存在则返回包含那个section下key的Option指针
**/
Option *cnf_has_option(Config *cnf, const char *section, const char *key);

/**
* 将Config对象写入指定文件中
* header表示在文件开头加一句注释
* 写入成功则返回true
**/
bool cnf_write_file(Config *cnf, const char *filename, const char *header);

/**
* 删除option
**/
bool cnf_remove_option(Config *cnf, const char *section, const char *key);

/**
* 删除section
**/
bool cnf_remove_section(Config *cnf, const char *section);

/**
* 销毁Config对象
* 删除所有数据
**/
void destroy_config(Config **cnf);

/**
* 打印当前Config对象
**/
void print_config(Config *cnf);

#endif
