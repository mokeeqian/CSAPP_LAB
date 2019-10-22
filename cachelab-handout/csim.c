/**
	Brife: cache lab source code (partA)
	Author: Jipeng Qian
	Date: 2019-10-3
*/


#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
//#include <math.h>

const int MAGIC_LRU_VALUE = 999;
typedef unsigned long long int memAddr;

//-----------------------------数据结构设计--------------------------------//
// 结果封装
typedef struct
{
	int hit;
	int miss;
	int evict;
}Result;

// 缓存行
typedef struct
{
	int valid;		// 有效位
	int tag;		// 标示位
	int data;	// 数据位(时间)	--> LRU counter, 初始化都是0
}cacheLine;

// 缓存组
typedef struct
{
	cacheLine *line;	// 一组中的所有行, 一个数组
}cacheSet;

// 缓存
typedef struct
{
	int numSets;		// cache中set总数目
	int numLinesPerSet;	// cache中每一个set中的行数
	cacheSet *set;		// 一个cache中的所有set,一个数组
}simCache;
//--------------------------------------------------------------------------//

simCache *initCache(int s, int b, int E);
int getMinLRUIndex(simCache *cache, int setBit);
void updateDataBit(simCache *cache, int setBit, int hitIndex);
int isHit(simCache *cache, int setBit, int tagBit);
int checkUpdate(simCache *cache, int setBit, int tagBit);
void accessCacheData(simCache *cache, Result *res, int addr, int size, int setBit, int tagBit);
void freeCacheAll(simCache *cache);

/*
	Main
*/
int main(int argc, char **argv)
{
	simCache *cache = NULL;
	Result res = {0,0,0};
	char ch;			// 选项
	int E=0,b=0,s=0;
	FILE *trace_file = NULL;
	int verbose = 0;	// v option
	char op;	// 操作
	int addr;		// 操作地址
	int size;		// 读取数据块大小
	const char *option = "hvs:E:b:t:";
	const char *help = "help message, to be completed...";
	
	// 输入参数解析
	while ( (ch = getopt(argc, argv, option)) != -1 )
	{
		switch(ch)
		{
			case 'h':
			{
				printf("%s\n", help);
				exit(0);
				break;
			}
			case 'v':
			{
				verbose = 1;
				break;
			}
			case 'E':
			{
				E = atoi(optarg);
				break;
			}
			case 'b':
			{
				b = atoi(optarg);
				break;
			}
			case 's':
			{
				s = atoi(optarg);
				break;
			}
			case 't':
			{
				if ( (trace_file = fopen(optarg, "r")) == NULL )
				{
					perror("[error] failed to open trace file!");
					exit(1);
				}
				break;
			}
			default:
			{
				printf("%s\n", help);
				exit(1);
				break;
			}
		}
	}
	
	cache = initCache(s, b, E);
	
	// 读文件
	while( fscanf(trace_file, " %c %x,%d", &op, &addr, &size) != EOF )
	{
		
		if ( verbose )
			printf("%c %x,%d\n", op, addr, size);
		// 这里不懂!!!
		int setMask = (1<<s)-1;
		int setIndex = setMask & (addr >> b);
		int tagMask = s + b;
		int tagBit = addr >> tagMask;
		
		//int tagBitSize = 64 - s - b; 		// m-s-b, 这里是64位机器
		//int inputTag = addr >> (s+b);
		//int setIndex = (addr << tagBitSize) >> (64-s);
		
		printf("%d\n", setIndex);
		
		if ( op == 'L' )
			accessCacheData(cache, &res, addr, size, setIndex, tagBit);
		else if ( op == 'S' )
			accessCacheData(cache, &res, addr, size, setIndex, tagBit);
		else if ( op == 'M' )
		{
			accessCacheData(cache, &res, addr, size, setIndex, tagBit);
			accessCacheData(cache, &res, addr, size, setIndex, tagBit);
		}
		else if ( op == 'I' )
			continue;
		else
			printf("[error] no operation...\n");
	}
	
	freeCacheAll(cache);
	printSummary(res.hit, res.miss, res.evict);
	return 0;
}


/**
	初始化一个模拟cache
	s: 组索引位数目
	b: 主存块数目
	E: 每组行数
*/
simCache *initCache(int s, int b, int E)
{
	int i;
	int j;
	//simCache *cache = NULL;	// error 没有初始化内存空间，导致段错误
	simCache *cache = malloc(sizeof(simCache));		
	
	cache->numSets = 1 << s;	// pow(2,s)
	cache->numLinesPerSet = E;	
	// set数组分配空间
	cache->set = (cacheSet *)malloc(cache->numSets*sizeof(cacheSet));	
	// 对每一个set进行初始化
	for ( i = 0; i < cache->numSets; ++i )
	{
		// line数组分配空间
		cache->set[i].line = (cacheLine *)malloc(cache->numLinesPerSet*sizeof(cacheLine));
		// 对每一个line进行初始化
		for ( j = 0; j < cache->numLinesPerSet; ++j )
		{
			cache->set[i].line[j].valid = 0;
			// cache->set[i].line[j].tag = 0;
			cache->set[i].line[j].data = 0;
		}
	}
	
	return cache;
}

/**
	获取最小的LRU数据位所在的index，作为evict行，用新的mem block来替换
	cache: 
	setBit:	
*/
int getMinLRUIndex(simCache *cache, int setBit)
{
	int index = 0;
	int min = MAGIC_LRU_VALUE;
	int i;
	for ( i = 0; i < cache->numLinesPerSet; ++i )
	{
		if ( cache->set[setBit].line[i].data < min )
		{
			index = i;
			min = cache->set[setBit].line[i].data;
		}
	}
	return index;
}

/**
	更新缓存LRU数据位，如果hit，就设置为设定的MAGIC_LRU_VALUE；否则，减一
	cache: 指向cache的指针变量
	setBit: 给定地址所在的组号
	hitIndex: hit 所在的行号
*/
void updateDataBit(simCache *cache, int setBit, int hitIndex)
{
	int i;

	cache -> set[setBit].line[hitIndex].data = MAGIC_LRU_VALUE;
	
	for ( i = 0; i < cache->numLinesPerSet; ++i )
	{
		if ( i != hitIndex )	// 不是命中行，减一
		{
			cache -> set[setBit].line[i].data --;
		}
	}
}

/**
	判断是否命中，要满足: vaild == 1 && tag == tagBit
	cache: 指向cache的指针变量
	setBit: 给定地址所在的组号(index)
	tagBit: 给定的tag位
*/
int isHit(simCache *cache, int setBit, int tagBit)
{
	int i;
	int hit = 0;
	for ( i = 0; i < cache -> numLinesPerSet; ++i )
	{
		// hit
		if ( cache -> set[setBit].line[i].valid == 1 && cache -> set[setBit].line[i].tag == tagBit )
		{	
			updateDataBit(cache, setBit, i);
			hit = 1;
		}
	}
	// miss
	return hit;
}

/**
*/
int checkUpdate(simCache *cache, int setBit, int tagBit)
{
	int i;
	int full = 1;
	for ( i = 0; i < cache->numLinesPerSet; ++i )
	{
		if ( cache->set[setBit].line[i].valid == 0 )		// 该组未满，因为初始化的就是0
		{
			full = 0;
			break;
		}
	}
	if ( !full )	
	{
		cache->set[setBit].line[i].valid = 1;
		cache->set[setBit].line[i].tag = tagBit;
		updateDataBit(cache, setBit, i);		// 更新数据位
	}
	else
	{
		int evictIndex = getMinLRUIndex(cache, setBit);		// 牺牲行
		cache->set[setBit].line[evictIndex].valid = 1;
		cache->set[setBit].line[evictIndex].tag = tagBit;
		updateDataBit(cache, setBit, evictIndex);		// 更新数据位
	}
	return full;
}


/**
	访问一次cache
*/
void accessCacheData(simCache *cache, Result *res, int addr, int size, int setBit, int tagBit)
{
	// miss
	if ( !isHit(cache, setBit, tagBit) )
	{
		res->miss ++;
		// 检查当前是否已经满了，是否是需要牺牲
		if ( checkUpdate(cache, setBit, tagBit) == 1 )
			res->evict ++;	
	}	
	else
	{
		res->hit ++;
	}
	return;
}

/**
	释放cache所占内存资源
*/
void freeCacheAll(simCache *cache)
{
	if ( cache )
	{
		int i;
		// int j;
		for ( i = 0; i < cache->numSets; ++i )
		{
			free(cache->set[i].line);
		}
		free(cache->set);
		free(cache);
	}
	return;
}
