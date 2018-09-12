#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sys_update.h"
#include "utils_common.h"

#define CUR_VERSION_FILE	"/var/soft_version"
#define CUR_VERSION			"C_300.M01C02S01W01.D5820HAH.100"
#define DISK_UPDATE_FLAG	0x67736dff

typedef struct{
	unsigned int 	flag;
	char			version[128];
	int 			parts;			//������������ģ����
	unsigned int	length;			//�ܳ�
}update_head_t;

typedef struct{
	char			path[128];
	unsigned int	length;
}update_part_t;

void* sys_update_disk_impl(void* args);

int sys_update_disk_file_check(sys_update_t* update);
int sys_update_disk_file_format(sys_update_t* update);
int sys_update_disk_version_check(sys_update_t* update);

int sys_update_disk_update(sys_update_t* update);

///////////////////////////////////////////////////////////
sys_update_t* sys_update_create()
{
	sys_update_t* update = (sys_update_t*)malloc(sizeof(sys_update_t));
	memset(update, 0x0, sizeof(sys_update_t));
	
	update->disk_tid		= 0;
	update->dist_tid_stop 	= 1;
	update->disk_interval 	= 3;
	
	//��ȡ��ǰ�汾�� /var/soft_version
	FILE* fd = fopen(CUR_VERSION_FILE, "rb");
	if(fd != NULL)
	{
		fread(update->cur_version, 1, 128, fd);
		fclose(fd);
	}
	else
	{
		strcpy(update->cur_version, CUR_VERSION);
	}

	return update;
}

void sys_update_destory(sys_update_t* update)
{
	if(update == NULL)
		return;

	if(update->disk_tid != 0)
	{
		update->dist_tid_stop = 1;
		pthread_join(update->disk_tid, NULL);
	}

	free(update);
}

int sys_update_disk_info_set(sys_update_t* update, char* file, int interval)
{
	if(update == NULL || file == NULL)
		return -1;

	strcpy(update->disk_file, file);
	update->disk_interval = interval;
	
	return 0;
}

int sys_update_disk_start(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	if(update->disk_tid == 0)
	{
		update->dist_tid_stop = 0;
		if(pthread_create(&update->disk_tid, NULL, sys_update_disk_impl, (void*)update) != 0)
		{
			printf("pthread_create sys_update_disk_file_check error \n");
			return -1;
		}
	}
	
	return 0;
}

int sys_update_disk_stop(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	if(update->disk_tid != 0)
	{
		update->dist_tid_stop = 1;
		pthread_join(update->disk_tid, NULL);
	}

	update->disk_tid = 0;
	
	return 0;
}

int sys_update_remote_start(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	return 0;
}

int sys_update_remote_stop(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	return 0;
}


void* sys_update_disk_impl(void* args)
{	
	sys_update_t* update = (sys_update_t*)args;
	
	if(update == NULL)
		return NULL;

	int ret;
	int count = 0;
	while(update->dist_tid_stop != 1)
	{
		count++;
		if(count / 2 == update->disk_interval)
		{
			ret = sys_update_disk_file_check(update);
			count = 0;
		}

		//ִ�и���
		if(ret == 0)
		{
			ret = sys_update_disk_update(update);
		}

		usleep(500);
	}
	
	return NULL;
}

int sys_update_disk_file_check(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	int ret;
	// 1. �ļ��Ƿ����
	if(access(update->disk_file, 0) != 0)
		return -1;

	// 2. �ļ���ʽ�Ƿ���ȷ
	ret = sys_update_disk_file_format(update);
	if(ret != 0)
	{
		printf("update file format error\n");
		return -1;
	}
	
	// 3. ����汾�Ƿ����
	ret = sys_update_disk_version_check(update);
	
	return ret;
}

int sys_update_disk_file_format(sys_update_t* update)
{
	if(update == NULL)
		return -1;
	
	FILE* fd = fopen(update->disk_file, "rb");
	if(fd == NULL)	return -1;

	do
	{
		int rd;
		// 1. ��־λ
		update_head_t head;
		rd = fread(&head, sizeof(update_head_t), 1, fd);
		if(rd != 1) 
			break;

		if(head.flag != DISK_UPDATE_FLAG)
			break;

		strcpy(update->update_version, head.version);
		
		// 2. ���ݳ�
		int i;
		int total_length = 0;
		
		for(i=0; i<head.parts; i++)
		{
			update_part_t part;
			rd = fread(&part, sizeof(update_part_t), 1, fd);
			if(rd != 1)
			{
				close(fd);
				return -1;
			}
			
			total_length += part.length;
		}

		if(total_length != head.length)
			break;

		return 0;
	}while(0);
	
	close(fd);

	return -1;
}

int sys_update_disk_version_check(sys_update_t* update)
{
	if(update == NULL)
		return -1;
	// ��ǰ�汾�������汾Ӳ���汾������汾�����ͺ��Ӱ汾�ñȽ�
	// ����汾�Ŷ���: ������ʶ.Ӳ���汾.����.����汾.release (C_300.M01C02S01W01.D5820HAH.101)
	int ret;
	
	char cur_flag[32];
	char cur_hard[32];
	char cur_modle[32];
	int  cur_soft[32];
	ret = sscanf(update->cur_version, "%s.%s.%s.%d", cur_flag, cur_hard, cur_modle, cur_soft);
	if(ret != 4)
		return -1;
	
	char update_flag[32];
	char update_hard[32];
	char update_modle[32];
	int  update_soft[32];
	ret = sscanf(update->update_version, "%s.%s.%s.%d", cur_flag, cur_hard, cur_modle, cur_soft);
	if(ret != 4)
		return -1;

	if(strcmp(cur_flag, update_flag) != 0)
	{
		return -1;
	}

	if(strcmp(cur_hard, update_hard) != 0)
	{
		return -1;
	}

	if(strcmp(cur_modle, update_modle) != 0)
	{
		return -1;
	}

	if(cur_soft >= update_soft)
	{
		return -1;
	}
	
	return 0;
}


int sys_update_disk_update(sys_update_t* update)
{
	if(update == NULL)
		return -1;

	int ret;
	// 1. ��ѹ�ļ����豸
	ret = sys_update_disk_overwrite(update);

	// 2. �޸İ汾��?
	char cmd[128] = {0};
	sprintf(cmd, "echo %s > %s", update->update_version, CUR_VERSION_FILE);
	ret = exec_cmd(cmd);

	// 3. ������־
	if(ret == 0)
	{
		sprintf(cmd, "echo success > %supdate.%s.result", DISK_PATH, update->update_version);
	}
	else
	{
		sprintf(cmd, "echo failure > %supdate.%s.result", DISK_PATH, update->update_version);
	}
	exec_cmd(cmd);
	
	if(ret == 0)
	{
		// 4. ����������
		sprintf(cmd, "mv %s %s.bak", update->disk_file, update->disk_file);
		exec_cmd(cmd);

		// 5. ����
		exec_cmd("reboot");
	}
	
	return 0;
}

int sys_update_disk_overwrite(sys_update_t* update)
{
	if(update == NULL)
		return -1;
	
	FILE* fd = fopen(update->disk_file, "rb");
	if(fd == NULL)	return -1;

	do
	{
		int rd;
		update_head_t head;
		rd = fread(&head, sizeof(update_head_t), 1, fd);
		if(rd != 1) 
			break;
		
		int i;
		for(i=0; i<head.parts; i++)
		{
			update_part_t part;
			rd = fread(&part, sizeof(update_part_t), 1, fd);
			if(rd != 1 || part.length == 0)
			{
				close(fd);
				return -1;
			}
//////////////////////////////////////////////////////////////////
			//ȡ��ÿ���ļ���������tmp�ļ�
			char tmp_file[128];
			char data[1024];
			
			sprintf(tmp_file, "/tmp/%s_tmp", part.path);
			FILE* fd_w = fopen(tmp_file, "wb");
			if(fd_w == NULL)	
			{
				close(fd);
				return -1;
			}

			int rd_left = part.length;
			while(rd_left > 0)
			{
				if(rd_left > 1024)
				{
					rd = 1024;
				}
				else
				{
					rd = rd_left;
				}
				rd = fread(data, 1, rd, fd);
				fwrite(data, 1, rd, fd_w);

				rd_left -= rd;
			}
			fclose(fd_w);

			// mvָ��ǽ��и���
			char cmd[128] = {0};
			sprintf(cmd, "mv tmp_file %s", part.path);
			exec_cmd(cmd);
//////////////////////////////////////////////////////////////////
		}

		return 0;
	}while(0);
	
	close(fd);
	return -1;
}

