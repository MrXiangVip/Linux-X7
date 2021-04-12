/**************************************************************************
 * 	FileName:	 pmgr.cpp
 *	Description:	user management
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		
 *	Created:		
 *	Updated:		
 *					
**************************************************************************/
#include <unistd.h>
#include "libsimplelog.h"
#include "pmgr.h"

PersonManager p_person_mgr;

//generate a person struct
Person genPerson(long long person_id,int person_type,const char* thumbnail) 
{
	Person person;

	person.personId = person_id;
	person.personType = person_type;
	//person.securityType = security_type;
	//strcpy(person.userName, user_name);
	//strcpy(person.userPwd, user_pwd);
	strcpy(person.thumbnail, thumbnail);

	return person;
}

bool PersonManager::isUserExisted(long long person_id) 
{
	PERSON_MAP::iterator it = m_person.find(person_id);
	if (it == m_person.end()) 
	{
		cerr << "User " << person_id << " doesn't have face" << endl;
		return false;
	} 
	else 
	{
		cout << "User " << person_id << " has face" << endl;
		return true;
	}
}

bool PersonManager::deleteUserById(int person_id) 
{
	PERSON_MAP::iterator it = m_person.find(person_id);
	if (it == m_person.end()) 
	{
		cerr << "User " << person_id << " doesn't have face" << endl;
		return false;
	} 
	else 
	{
		m_person.erase(it);
		cout << "User " << person_id << " has been deleted" << endl;
		bool res = savePersonManager();
		if (res) 
		{
			cout << "User " << person_id << " has been deleted and save to db successfully" << endl;
			return true;
		} 
		else 
		{
			cerr << "Delete User " << person_id << " OK but save to db fail" << endl;
			return true;
		}
	}
}

bool PersonManager::isUserExisted(const char* user_name) 
{
	PERSON_MAP::iterator it;
	for(it = m_person.begin(); it != m_person.end(); ++it) 
	{
		cout<<"key1: "<<it->first <<" value: "<< endl;
		Person person = it->second;
		if (strcmp(person.userName, user_name) == 0) 
		{
			return true;
		}
	}
	return false;
}

int PersonManager::getMaxPersonId() 
{
	PERSON_MAP::iterator it;
	int max_person_id = 0;
	for(it = m_person.begin(); it != m_person.end(); ++it) 
	{
		if (max_person_id < it->first) 
		{
			max_person_id = it->first;
		}
	}
	return max_person_id;
}

bool PersonManager::registerUser(Person person) 
{
	if (m_person.size() > PERSON_MAX_COUNT) 
	{
		cerr << "the number of user has exceeded " << PERSON_MAX_COUNT << endl;
		return false;
	}
	if (isUserExisted(person.personId)) 
	{ 
		cerr << "user " << person.personId << " already exist" << endl;
		return false;
	}
	if (isUserExisted(person.userName)) 
	{
		cerr << "user " << person.userName << " already exist" << endl;
		return false;
	}
	int max_person_id = getMaxPersonId();
	int new_person_id = max_person_id + 1;
	m_person.insert(map<int, Person>::value_type(new_person_id, person));
	int ret = savePersonManager();
	if (!ret) 
	{
		cerr << "Failed to save data" << endl;
		return false;
	}
	cout << "register user " << new_person_id << " successfully, current user number " << m_person.size() << endl;
	return true;
}

bool PersonManager::updateUser(Person person) 
{
	int person_id = person.personId;
	PERSON_MAP::iterator it = m_person.find(person_id);
	if (it == m_person.end()) 
	{
		cout << "Fail to update due to user " << person_id << " doesn't exist" << endl;
		return false;
	} 
	else 
	{
		m_person[person_id] = person;

		int ret = savePersonManager();
		if (ret != 0) 
		{
			cerr << "Failed to save data" << endl;
			return false;
		}
		return true;
	}
}

bool PersonManager::savePersonManager() 
{
	return savePersonManager(PERSON_DB_FILE);
}
bool PersonManager::loadPersonManager() 
{
	return loadPersonManager(PERSON_DB_FILE);
}

bool PersonManager::loadPersonManager(const char* file_name) 
{
    printf("%s \n",__func__);
	m_person.clear();

	FILE *fp;
	if (!(fp = fopen(file_name, "r")))  
	{
		printf("file %s open fail\n", file_name);
		return false;
	}
	printf("File %s open ok\n", file_name);
#if 1
	int headerLen = sizeof(Pm_FileHeader);
	Pm_FileHeader current_file_header;
	size_t readLen = fread(&m_file_header, 1, headerLen, fp);
	cout << "headerLen is " << readLen << endl;
	if (headerLen != readLen) 
	{
		cerr << "Failed to read file header" << endl;
		fclose(fp);
		return false;
	}
	int curr_pos = readLen;
#endif
	cout << "Person File Header:"<< current_file_header << endl;

	int currentCount = m_file_header.currentCount;
	for (int i = 0; i < currentCount; i++) 
	{
		// printf("offset = %d\n", curr_pos);
		Person person;
		size_t pLen = fread(&person, 1, sizeof(Person), fp);
		if (pLen != sizeof(Person)) 
		{
			cerr << "Failed to read person" << endl;
			fclose(fp);
			return false;
		}
		curr_pos += pLen;
		cout << "person: " << person << endl;
		m_person.insert(map<int, Person>::value_type(person.personId, person));
	}

	fclose(fp);
	return true;
}

bool PersonManager::savePersonManager(const char* file_name) 
{
	FILE *fp;
	if (!(fp = fopen(file_name, "wb")))  
	{
		printf("file %s open fail\n", file_name);
		return false;
	}
	log_debug("file %s open ok\n", file_name);

	m_file_header.headerSize = sizeof(Pm_FileHeader);
	m_file_header.maxCount = PERSON_MAX_COUNT;
	m_file_header.currentCount = m_person.size();
	m_file_header.version = VERSION_V1;
	m_file_header.reserved = 0;

	cout << m_file_header << endl;
	fwrite(&m_file_header, sizeof(Pm_FileHeader), 1, fp);

	PERSON_MAP::iterator it;
	for(it = m_person.begin(); it != m_person.end(); ++it) 
	{
		Person person = it->second;
		cout<<"person "<<person.personId << ":\n\t" << person << endl;
		int person_len = sizeof(Person);
		fwrite(&person, sizeof(Person), 1, fp);
	}

	/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
	fflush(fp); 	/* 刷新内存缓冲，将内容写入内核缓冲 */
	fsync(fileno(fp));	/* 将内核缓冲写入磁盘 */
	fclose(fp);
	return true;
}

