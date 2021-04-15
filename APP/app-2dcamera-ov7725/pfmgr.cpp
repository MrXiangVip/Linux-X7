/**************************************************************************
 * 	FileName:	 pfmgr.cpp
 *	Description:	face management
 *	Copyright(C):	2018-2020 Wave Group Inc.
 *	Version:		V 1.0
 *	Author:		
 *	Created:		
 *	Updated:		
 *					
**************************************************************************/
#include <unistd.h>
#include "libsimplelog.h"
#include "pfmgr.h"

PersonFaceManager p_person_face_mgr;

SFaceFt* PersonFaceManager::getFaceFtById(int person_id) 
{
	PERSONFACE_MAP::iterator it = m_person_faces.find(person_id);
	if (it == m_person_faces.end()) {
		// cerr << "用户" << person_id << "不存在" << endl;
		cerr << "User " << person_id << " Not Exist" << endl;
		return NULL;
	} else {
		PersonFace person = it->second;
		SFaceFt* face = person.face;
		return face;
	}
}

SFaceFt* PersonFaceManager::getFaceFtByUUID(uUID uu_id) 
{
	map<int, PersonFace>::iterator it;
	int i = 0;
	for(it = m_person_faces.begin(); it != m_person_faces.end(); ++it) 
	{
		PersonFace person = it->second;
		if(!memcmp(&uu_id, &person.uu_id, sizeof(uUID)))
		{
			SFaceFt *face = person.face;
			return face;
		}
	}

	return NULL;
}

bool PersonFaceManager::hasPersonFace(int person_id) 
{
	PERSONFACE_MAP::iterator it = m_person_faces.find(person_id);
	if (it == m_person_faces.end()) {
		cerr << "User " << person_id << " doesn't have face" << endl;
		return false;
	} else {
		cout << "User " << person_id << " has face" << endl;
		return true;
	}
}

bool PersonFaceManager::deletePersonById(int person_id) 
{
	PERSONFACE_MAP::iterator it = m_person_faces.find(person_id);
	if (it == m_person_faces.end()) 
	{
		cerr << "User " << person_id << " doesn't have face" << endl;
		return false;
	}
	else 
	{
		m_person_faces.erase(it);
		cout << "User " << person_id << " has been deleted" << endl;
		bool res = savePersonFaceMgr();
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

bool PersonFaceManager::deletePersonByUUID(uUID uu_id) 
{
	map<int, PersonFace>::iterator it;
	int i = 0;
	for(it = m_person_faces.begin(); it != m_person_faces.end(); ++it) 
	{
		PersonFace person = it->second;
		if(!memcmp(&uu_id, &person.uu_id, sizeof(uUID)))
		{
			log_info("reg person.uu_id: H<0x%08x>, L<0x%08x>.\n", person.uu_id.tUID.H, person.uu_id.tUID.L);
			m_person_faces.erase(it);
			bool res = savePersonFaceMgr();
			if (res) 
			{
				log_info("uuid<0x%08x-0x%08x> has been deleted and save to db successfully.\n",  person.uu_id.tUID.H,person.uu_id.tUID.L);
				return true;
			} 
			else 
			{
				log_error("Delete uuid<0x%08x-0x%08x> OK but save to db fail.\n",  person.uu_id.tUID.H,person.uu_id.tUID.L);
				return false;
			}
		}
	}

	return false;
}


int PersonFaceManager::getPersonIdByFace(SFaceFt* face_ft) 
{
	return -1;
}

PersonFace genPerson(int person_id, SFaceFt* faceFt, uUID * pUU_ID) 
{
 	PersonFace person;
 	person.personId = person_id;
	memcpy(&person.uu_id, pUU_ID, sizeof(uUID));
	
 	person.face = faceFt;
	return person;
}	

REG_UserFace_State PersonFaceManager::registerFaceFt(int person_id, SFaceFt* faceFt, uUID * pUU_ID)
{
	PERSONFACE_MAP::iterator it = m_person_faces.find(person_id);
	if (it == m_person_faces.end()) 
	{
		// cout << "新增注册用户" << endl;
		cout << "New user" << endl;

		PersonFace person = genPerson(person_id, faceFt, pUU_ID);
		m_person_faces.insert(map<int, PersonFace>::value_type(person_id, person));

		int ret = savePersonFaceMgr();
		if (!ret) 
		{
			// cerr << "保存数据失败" << endl;
			cerr << "Failed to save data" << endl;
			return Pf_DB_SAVE_FAILD;
		}
		// cout << "注册用户" << person_id << "成功，当前用户数" << m_person_faces.size() << endl;
		cout << "register PersonID:" << person_id << " successfully，current user number " << m_person_faces.size() << endl;
		return Pf_REG_USER_OK;
	} 
	else 
	{
		 cout << "注册用户" << person_id << "已经存在" << endl;
		 return Pf_ALREAD_EXIT;
	}
}

REG_UserFace_State PersonFaceManager::updateFaceFt(int person_id, SFaceFt* faceFt, uUID * pUU_ID) 
{
	PERSONFACE_MAP::iterator it = m_person_faces.find(person_id);
	if (it == m_person_faces.end()) 
	{
		// cout << "无法更新，用户" << person_id << "不存在" << endl;
		cout << "Fail to update due to user " << person_id << " doesn't exist" << endl;
		return Pf_NOT_EXIT;
	} 
	else 
	{
		PersonFace person;
		person.personId = person_id;
		person.face = faceFt;
		m_person_faces[person_id] = person;

		int ret = savePersonFaceMgr();
		if (ret != 0) 
		{
			// cerr << "保存数据失败" << endl;
			cerr << "Failed to save data" << endl;
			return Pf_DB_SAVE_FAILD;
		}
		return Pf_REG_USER_OK;
	}
}

bool PersonFaceManager::savePersonFaceMgr() 
{
	return savePersonFaceMgr(PERSON_DB_FILE);
}
bool PersonFaceManager::loadPersonFaceMgr() 
{
	return loadPersonFaceMgr(PERSON_DB_FILE);
}

bool PersonFaceManager::loadPersonFaceMgr(const char* file_name) 
{
    printf("%s \n", __func__);
	m_person_faces.clear();
	m_facefts = NULL;

	FILE *fp;
	if (!(fp = fopen(file_name, "r")))  
	{
		printf("file %s open fail\n", file_name);
		return false;
	}
	printf("File %s open ok\n", file_name);
#if 1
	int headerLen = sizeof(Pf_FileHeader);
	Pf_FileHeader current_file_header;
	size_t readLen = fread(&m_file_header, 1, headerLen, fp);
	cout << "readLen is " << readLen << "-" << sizeof(SFaceFt)<< endl;
	if (headerLen != readLen) 
	{
		// cerr << "读取文件头失败" << endl;
		cerr << "Failed to read file header" << endl;
		fclose(fp);
		return false;
	}
	int curr_pos = readLen;
#endif
#if 0
	cout << current_file_header.headerSize << endl;
	cout << current_file_header.maxCount << endl;
	cout << current_file_header.currentCount << endl;
	cout << current_file_header.oneFaceSize << endl;
	cout << current_file_header.reserved<< endl;
#endif

	int currentCount = m_file_header.currentCount;
	for (int i = 0; i < currentCount; i++) 
	{
		// printf("offset = %d\n", curr_pos);
		PersonFace person;
		size_t pLen = fread(&person, 1, sizeof(PersonFace), fp);
		if (pLen != sizeof(PersonFace)) 
		{
			// cerr << "读取person头失败" << endl;
			cerr << "Failed to read person header" << endl;
			fclose(fp);
			return false;
		}
		curr_pos += pLen;
		// printf("offset = %d\n", curr_pos);
#if 0
		SFaceFt faceFt;
		size_t fLen = fread(&faceFt, 1, sizeof(SFaceFt), fp);
		if (fLen != sizeof(SFaceFt)) {
			cerr << "读取Face头失败" << endl;
			fclose(fp);
			return -1;
		}
		int nLocalFtLen = faceFt.nLocalFtLen;
		cout << "loacalFtLen is " << nLocalFtLen << endl;
		cout << "nFaceCount is " << faceFt.nFaceCount << endl;
		cout << "nversion is " << faceFt.nVersion << endl;
		cout << "nTimeUpdate is " << faceFt.nTimeUpdate << endl;
#else
		SFaceFt *faceFt = new SFaceFt();
		size_t fLen = fread(faceFt, 1, sizeof(SFaceFt), fp);
		if (fLen != sizeof(SFaceFt)) {
			// cerr << "读取Face头失败" << endl;
			cerr << "Failed to read face header" << endl;
			fclose(fp);
			return false;
		}
		curr_pos += fLen;
		// printf("offset = %d\n", curr_pos);
		int nLocalFtLen = faceFt->nLocalFtLen;
#if 0
		cout << "loacalFtLen is " << nLocalFtLen << endl;
		cout << "nFaceCount is " << faceFt->nFaceCount << endl;
		cout << "nversion is " << faceFt->nVersion << endl;
		cout << "nTimeUpdate is " << faceFt->nTimeUpdate << endl;
#endif

#endif 
		int feature_len = PERSON_FEATURE_NUM * sizeof(float) * FACE_FEATURE_LEN;
		//cout << "read feature_len:" << feature_len << endl;
		byte *pbLocalFt = new byte[feature_len];
		memset(pbLocalFt, '\0', feature_len);
		size_t ftLen = fread(pbLocalFt, 1, feature_len, fp);
		if (ftLen != feature_len) 
		{
			cout << "ftLen " << ftLen << " feature_len " << feature_len<< endl;
			// cerr << "读取Face feature数据失败" << endl;
			cerr << "failed to read Face feature data" << endl;
			fclose(fp);
			return false;
		}
		curr_pos += ftLen;
		// printf("offset = %d\n", curr_pos);
#if 0
		faceFt.pbLocalFt = pbLocalFt;
		personId.face = &faceFt;
		printf("--- %p \n ", &faceFt);
#else
		faceFt->pbLocalFt = pbLocalFt;
		if (nLocalFtLen > feature_len) 
		{
			faceFt->nLocalFtLen = feature_len;
		}
		person.face = faceFt;
		// printf("--- %p \n ", faceFt);
#endif
		//cout << "person: " << person << endl;
		//if (person.face == NULL) cout << "null" << endl; else cout << "not null" << endl;
		m_person_faces.insert(map<int, PersonFace>::value_type(person.personId, person));
	}

	fclose(fp);
	return true;
}

bool PersonFaceManager::savePersonFaceMgr(const char* file_name) 
{
    printf("%s \n", __func__);
    FILE *fp;
	if (!(fp = fopen(file_name, "wb")))  
	{
		printf("file %s open fail\n", file_name);
		return false;
	}
	log_debug("file %s open ok\n", file_name);

	m_file_header.headerSize = sizeof(Pf_FileHeader);
	m_file_header.maxCount = PERSON_MAX_COUNT;
	m_file_header.currentCount = m_person_faces.size();
	m_file_header.oneFaceSize = sizeof(SFaceFt) + sizeof(float) * FACE_FEATURE_LEN * PERSON_FEATURE_NUM;

	cout << m_file_header << endl;
	fwrite(&m_file_header, sizeof(Pf_FileHeader), 1, fp);

	PERSONFACE_MAP::iterator it;
	log_debug("%s has person num:%d\n", PERSON_DB_FILE,m_person_faces.size());
	for(it = m_person_faces.begin(); it != m_person_faces.end(); ++it) 
	{
		PersonFace person = it->second;
		SFaceFt *face = person.face;
		// 如果face不存在
		if (face == NULL) 
		{
			int face_len = sizeof(SFaceFt) + PERSON_FEATURE_NUM * sizeof(float) * FACE_FEATURE_LEN;
			fwrite(&person, sizeof(PersonFace), 1, fp);

			char buf[face_len];
			memset(buf, '\0', sizeof(buf));
			fwrite(buf, sizeof(buf), 1, fp);
		}
		else 
		{
			int face_feature_count = face->nFaceCount;
			if (face_feature_count > PERSON_FEATURE_NUM) 
			{
				face->nLocalFtLen = face->nLocalFtLen * PERSON_FEATURE_NUM / face_feature_count;
				face_feature_count = PERSON_FEATURE_NUM;
				face->nFaceCount = face_feature_count;
			}

			// person.face = face;
			fwrite(&person, sizeof(PersonFace), 1, fp);

			int face_len = sizeof(SFaceFt) + PERSON_FEATURE_NUM * sizeof(float) * FACE_FEATURE_LEN;

			char buf[face_len];
			memset(buf, '\0', sizeof(buf));
			memcpy(buf, face, sizeof(SFaceFt));
			if (face->pbLocalFt != NULL)
			memcpy(buf + sizeof(SFaceFt), face->pbLocalFt, face->nLocalFtLen);
			fwrite(buf, sizeof(buf), 1, fp);
		}
	}

	/* 将文件同步,及时写入磁盘,不然，系统重启动，导致文件内容丢失 */
	fflush(fp); 	/* 刷新内存缓冲，将内容写入内核缓冲 */
	fsync(fileno(fp));	/* 将内核缓冲写入磁盘 */
	fclose(fp);
	return true;
}

SFaceFt** PersonFaceManager::getPersonFaces() 
{
	if (m_facefts != NULL) 
	{
		delete m_facefts;
	}
	m_facefts = new SFaceFt*[m_person_faces.size()];
	map<int, PersonFace>::iterator it;
	int i = 0;
	for(it = m_person_faces.begin(); it != m_person_faces.end(); ++it) 
	{
		PersonFace person = it->second;
		SFaceFt *face = person.face;
		m_facefts[i++] = face;
	}
	return m_facefts;
}

