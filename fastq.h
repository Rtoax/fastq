/**********************************************************************************************************************\
*  文件： fastq.h
*  介绍： 低时延队列
*  作者： 荣涛
*  日期：
*       2021年1月   - 2021年5月
*
* API接口概述
*
*   FastQCreateModule   注册消息队列
*   FastQDeleteModule   删除消息队列
*   FastQDump           显示信息
*   FastQDumpAllModule  显示信息（所有模块）
*   FastQMsgStatInfo    查询队列内存入队出队信息
*   FastQSend           发送消息（轮询直至成功发送）
*   FastQSendByName         模块名索引版本
*   FastQTrySend        发送消息（尝试向队列中插入，当队列满是直接返回false）
*   FastQTrySendByName      模块名索引版本
*   FastQRecv           接收消息
*   FastQMsgNum         获取消息数(需要开启统计功能 _FASTQ_STATS )
*   FastQAddSet         动态添加 发送接收 set
*
*
\**********************************************************************************************************************/
#ifndef __fAStMQ_H
#define __fAStMQ_H 1

#include <stdio.h>
#include <stdbool.h>



#ifdef MODULE_ID_MAX // moduleID 最大模块索引值
#define FASTQ_ID_MAX    MODULE_ID_MAX
#else
#define FASTQ_ID_MAX    256
#endif

/**
 *  Crypto
 */
#define __MOD_SETSIZE  FASTQ_ID_MAX
#define __NMOD     (8 * (int) sizeof (__mod_mask))
#define __MOD_ELT(d)   ((d) / __NMOD)
#define __MOD_MASK(d)  ((__mod_mask)(1UL << ((d) % __NMOD)))

typedef long int __mod_mask;
typedef struct {
    __mod_mask __mod[__MOD_SETSIZE/__NMOD];
#define __MOD(set) ((set)->__mod)
#define __MOD_SET_INITIALIZER    {0}
}__attribute__((aligned(64))) __mod_set;


#define __MOD_ZERO(s) \
    do {    \
        unsigned int __i;   \
        __mod_set *__arr = (s);  \
        for (__i = 0; __i < sizeof (__mod_set) / sizeof (__mod_mask); ++__i)    \
            __MOD (__arr)[__i] = 0;  \
    } while (0)
#define __MOD_SET(d, s) \
    ((void) (__MOD (s)[__MOD_ELT(d)] |= __MOD_MASK(d)))
#define __MOD_CLR(d, s) \
    ((void) (__MOD (s)[__MOD_ELT(d)] &= ~ __MOD_MASK(d)))
#define __MOD_ISSET(d, s) \
    ((__MOD (s)[__MOD_ELT (d)] & __MOD_MASK (d)) != 0)


#define mod_set                  __mod_set
#define MOD_SET_INITIALIZER     __MOD_SET_INITIALIZER

#define MOD_SETSIZE                    __MOD_SETSIZE
#define MOD_SET(bit, p_mod_set)       __MOD_SET(bit, p_mod_set)
#define MOD_CLR(bit, p_mod_set)       __MOD_CLR(bit, p_mod_set)
#define MOD_ISSET(bit, p_mod_set)     __MOD_ISSET(bit, p_mod_set)
#define MOD_ZERO(p_mod_set)           __MOD_ZERO(p_mod_set)


/**
 *  源模块未初始化时可临时使用的模块ID，只允许使用一次
 *
 *  目前 0 号 不允许使用
 */
#define FastQTmpModuleID    0


/**
 *  FastQModuleMsgStatInfo - 统计信息
 *
 *  src_module  源模块ID
 *  dst_module  目的模块ID
 *  enqueue     从 src_module 发往 dst_module 的统计， src_module 已发出的消息数
 *  dequeue     从 src_module 发往 dst_module 的统计， dst_module 已接收的消息数
 */
struct FastQModuleMsgStatInfo {
    unsigned long src_module;
    unsigned long dst_module;
    unsigned long enqueue;
    unsigned long dequeue;
};


/**
 *  fq_msg_handler_t - FastQRecvMain 接收函数
 *
 *  param[in]   src     源模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   dst     目的模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   type    消息类型
 *  param[in]   code    消息码
 *  param[in]   subcode 次消息码
 *  param[in]   msg     接收消息地址
 *  param[in]   sz      接收消息大小，与 FastQCreate (..., msg_size) 保持一致
 */
typedef void (*fq_msg_handler_t)(unsigned long src, unsigned long dst,\
                                 unsigned long type, unsigned long code, unsigned long subcode, \
                                 void*msg, size_t sz);


/**
 *  fq_module_filter_t - 根据目的和源模块ID进行过滤
 *
 *  param[in]   srcID 源模块ID
 *  param[in]   dstID 目的模块ID
 *
 *  当 fq_module_filter_t 返回 true 时，该 源 到 目的 的消息队列将计入统计
 */
typedef bool (*fq_module_filter_t)(unsigned long srcID, unsigned long dstID);





/**
 *  FastQCreateModule - 注册消息队列
 *
 *  param[in]   moduleID    模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   rxset       可能接收对应模块发来的消息 bitmap，见 select() fd_set
 *  param[in]   txset       可能向对应模块发送消息 bitmap，见 select() fd_set
 *  param[in]   msgMax      该模块 的 消息队列 的大小
 *  param[in]   msgSize     最大传递的消息大小
 */
void
FastQCreateModule(const unsigned long moduleID,
                         const mod_set *rxset, const mod_set *txset,
                         const unsigned int msgMax, const unsigned int msgSize);


/**
 *  FastQAttachName - 绑定 Name 到 ModuleID 消息队列, 以使用 Name发送消息
 *
 *  param[in]   name        模块名(长度 <= 64)
 *  param[in]   moduleID    模块ID， 范围 1 - FASTQ_ID_MAX
 */
bool
FastQAttachName(const unsigned long moduleID, const char *name);


/**
 *  FastQDeleteModule - 销毁消息队列
 *
 *  param[in]   moduleID    模块ID， 范围 1 - FASTQ_ID_MAX
 *
 *  return 成功true 失败false
 *
 *  需要注意的是，name 和 moduleID 二选一，但是，如果与注册时的对应关系不一致
 *  将销毁失败
 */
bool
FastQDeleteModule(const unsigned long moduleID);



/**
 *  FastQAddSet - 注册消息队列
 *
 *  param[in]   moduleID    模块ID， 范围 1 - FASTQ_ID_MAX, 通过 `FastQCreateModule` 注册的函数
 *  param[in]   rxset       可能接收对应模块发来的消息 bitmap，见 select() fd_set
 *  param[in]   txset       可能向对应模块发送消息 bitmap，见 select() fd_set
 *
 *  注意：这里的`rxset`和`txset`将是`FastQCreateModule`参数的并集
 */
bool
FastQAddSet(const unsigned long moduleID,
                    const mod_set *rxset, const mod_set *txset);


/**
 *  FastQDump - 显示信息
 *
 *  param[in]   fp    文件指针
 *  param[in]   module_id 需要显示的模块ID， 等于 0 时显示全部
 */
void
FastQDump(FILE*fp, unsigned long moduleID);

/**
 *  FastQDump - 显示全部模块信息
 *
 *  param[in]   fp    文件指针
 */
void
FastQDumpAllModule(FILE*fp);

/**
 *  FastQMsgStatInfo - 获取统计信息
 *
 *  param[in]   buf     FastQModuleMsgStatInfo 信息结构体
 *  param[in]   buf_mod_size    buf 信息结构体个数
 *  param[in]   num     函数返回时填回 的 FastQModuleMsgStatInfo 结构个数
 *  param[in]   filter  根据目的和源模块ID进行过滤 详见 fq_module_filter_t
 */

bool
FastQMsgStatInfo(struct FastQModuleMsgStatInfo *buf, unsigned int buf_mod_size, unsigned int *num,
                fq_module_filter_t filter);


/**
 *  FastQSend - 发送消息（轮询直至成功发送）
 *
 *  param[in]   from    源模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   to      目的模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   msgType 消息类型
 *  param[in]   msgCode 消息码
 *  param[in]   msgSubCode 次消息码
 *  param[in]   msg     传递的消息体
 *  param[in]   size    传递的消息大小
 *
 *  return 成功true （轮询直至发送成功，只可能返回 true ）
 *
 *  注意：from 和 to 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQSend(unsigned int from, unsigned int to, unsigned long msgType, unsigned long msgCode, unsigned long msgSubCode,
                const void *msg, size_t size);

/**
 *  FastQSendByName - 发送消息（轮询直至成功发送）
 *
 *  param[in]   from    源模块名
 *  param[in]   to      目的模块名
 *  param[in]   msgType 消息类型
 *  param[in]   msgCode 消息码
 *  param[in]   msgSubCode 次消息码
 *  param[in]   msg     传递的消息体
 *  param[in]   size    传递的消息大小
 *
 *  return 成功true （轮询直至发送成功，只可能返回 true ）
 *
 *  注意：from 和 to 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQSendByName(const char *from, const char *to, unsigned long msgType, unsigned long msgCode, unsigned long msgSubCode,
                 const void *msg, size_t size);


/**
 *  FastQTrySend - 发送消息（尝试向队列中插入，当队列满是直接返回false）
 *
 *  param[in]   from    源模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   to      目的模块ID， 范围 1 - FASTQ_ID_MAX
 *  param[in]   msgType 消息类型
 *  param[in]   msgCode 消息码
 *  param[in]   msgSubCode 次消息码
 *  param[in]   msg     传递的消息体
 *  param[in]   size    传递的消息大小
 *
 *  return 成功true 失败false
 *
 *  注意：from 和 to 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQTrySend(unsigned int from, unsigned int to, unsigned long msgType, unsigned long msgCode, unsigned long msgSubCode,
                 const void *msg, size_t size);


/**
 *  FastQTrySendByName - 发送消息（尝试发送）
 *
 *  param[in]   from    源模块名
 *  param[in]   to      目的模块名
 *  param[in]   msgType 消息类型
 *  param[in]   msgCode 消息码
 *  param[in]   msgSubCode 次消息码
 *  param[in]   msg     传递的消息体
 *  param[in]   size    传递的消息大小
 *
 *  return 成功true 失败false
 *
 *  注意：from 和 to 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQTrySendByName(const char *from, const char *to, unsigned long msgType, unsigned long msgCode, unsigned long msgSubCode,
                 const void *msg, size_t size);


/**
 *  FastQRecv - 接收消息
 *
 *  param[in]   from    从模块ID from 中读取消息， 范围 1 - FASTQ_ID_MAX
 *  param[in]   handler 消息处理函数，参照 fq_msg_handler_t 说明
 *
 *  return 成功true 失败false
 *
 *  注意：from 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQRecv(unsigned int from, fq_msg_handler_t handler);


/**
 *  FastQRecvByName - 接收消息
 *
 *  param[in]   from    源模块名
 *  param[in]   handler 消息处理函数，参照 fq_msg_handler_t 说明
 *
 *  return 成功true 失败false
 *
 *  注意：from 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQRecvByName(const char *from, fq_msg_handler_t handler);


/**
 *  FastQMsgNum - 获取消息数
 *
 *  param[in]   ID    从模块ID from 中读取消息， 范围 1 - FASTQ_ID_MAX
 *  param[in]   nr_enqueues 总入队数
 *  param[in]   nr_dequeues 总出队数
 *  param[in]   nr_currents 当前消息数
 *
 *  return 成功true 失败false(不支持, 编译宏控制 _FASTQ_STATS 开启统计功能)
 *
 *  注意：ID 需要使用 FastQCreateModule 注册后使用
 */
bool
FastQMsgNum(unsigned int ID, unsigned long *nr_enqueues, unsigned long *nr_dequeues, unsigned long *nr_currents);





# define FastQCreateModule(moduleID, rxset, txset, msgMax, msgSize)   \
             FastQCreateModuleDump(moduleID, rxset, txset, msgMax, msgSize, __FILE__, __func__, __LINE__)
# define FastQDumpAllModule(fp)                 \
             FastQDump(fp, 0)



#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wattributes"


void
FastQCreateModuleDump(const unsigned long moduleID,
                        const mod_set *rxset, const mod_set *txset,
                        const unsigned int msgMax, const unsigned int msgSize,
                            const char *_file, const char *_func, const int _line);

#pragma GCC diagnostic pop


#endif /*<__fAStMQ_H>*/


