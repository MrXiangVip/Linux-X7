#ifndef _PERSON_FACE_MGR_H_
#define _PERSON_FACE_MGR_H_

#include <iostream>
#include <map>
#include <stdio.h>
#include <memory.h>

#include "FIT.h"

using namespace::std;

#define PERSON_MAX_COUNT 200
#define PERSON_FEATURE_NUM 5
#define FACE_FEATURE_LEN 256
#define PERSON_DB_FILE "personface.db"

enum REG_UserFace_State {
  Pf_REG_USER_OK=0,            // 0  注册保存OK
  Pf_DB_SAVE_FAILD=-1,           // -1  保存识别
  Pf_ALREAD_EXIT=-2,       // -2  用户已经存在
  Pf_NOT_EXIT=-3,      // -3 用户不存在
};


/*8字节UUID*/
typedef union{
	struct{
		uint32_t L;
		uint32_t H;
	}tUID;
	uint8_t UID[8];
}uUID;

class Pf_FileHeader {
	public:
	int headerSize;
	int maxCount;
	int currentCount;
	int oneFaceSize;
	int reserved;
	Pf_FileHeader() {
		headerSize = 20;
		maxCount = PERSON_MAX_COUNT;
		currentCount = 0;
		oneFaceSize = 1024 * 5;
		reserved = 0;
	}
	friend ostream& operator<<(ostream & os,const Pf_FileHeader &fileHeader) {
		os << "Pf_FileHeader:" << endl;
		os << "\theaderSize: "<< fileHeader.headerSize << endl;
		os << "\tmaxCount: "<< fileHeader.headerSize << endl;
		os << "\tcurrentCount: "<< fileHeader.currentCount<< endl;
		os << "\toneFaceSize: "<< fileHeader.oneFaceSize<< endl;
		os << "\treserved: "<< fileHeader.reserved << endl;
		return os;
	}
};

class PersonFace {
	public:
	int personId;
	uUID uu_id;
	SFaceFt *face;
	PersonFace() {
		personId = -1;
		face = NULL;
	}
	friend ostream& operator<<(ostream & os,const PersonFace &person) {
		os << "PersonFace:" << endl;
		os << "\tpersonId: "<< person.personId<< endl;
		os << "\tuu_id: "<< person.uu_id.tUID.H <<" - "<< person.uu_id.tUID.L<< endl;
		if (person.face != NULL) {
			os << "\tSFaceFt: "<<endl;
			os << "\t\tnFaceCount: "<< person.face->nFaceCount<< endl;
			os << "\t\tnVersion: "<< person.face->nVersion<< endl;
			os << "\t\tnLocalFtLen: "<< person.face->nLocalFtLen<< endl;
			if (person.face->pbLocalFt != NULL) {
				os << "\t\tpbLocalFt: "<< (char*)(person.face->pbLocalFt)<< endl;
			}
			os << "\t\tnTimeUpdate: "<< person.face->nTimeUpdate<< endl;
		}
		return os;
	}
};

#if 0
struct SFaceFt1 {
  byte *pbLocalFt;    //本地特征数据
  short nFaceCount;   //人脸数
  short nVersion;     //人脸特征版本序号
  int nLocalFtLen;    //本地特征数据总长度
  int nTimeUpdate;    //特征更新时间
  SFaceFt1(void) {
    nVersion = 5;
    nFaceCount = 0;   //人脸数
    nLocalFtLen = 0;  //本地特征数据总长度
    pbLocalFt = 0;    //本地特征数据
    nTimeUpdate = 20000101;
  }
};
#endif

typedef map<int, PersonFace> PERSONFACE_MAP;

class PersonFaceManager {
public:
	SFaceFt* getFaceFtById(int personId);
	SFaceFt* getFaceFtByUUID(uUID uu_id);
	bool hasPersonFace(int personId);
	bool deletePersonById(int personId);
	bool deletePersonByUUID(uUID uu_id);
	REG_UserFace_State registerFaceFt(int personId, SFaceFt* faceFt, uUID * pUU_ID);
	REG_UserFace_State updateFaceFt(int personId, SFaceFt* faceFt, uUID * pUU_ID);
	// deletePerson(int person_id);
	int getPersonIdByFace(SFaceFt* faceFt);
// private:
public:
	bool loadPersonFaceMgr();
	bool loadPersonFaceMgr(const char* file_name);
	bool savePersonFaceMgr();
	bool savePersonFaceMgr(const char* file_name);
	SFaceFt** getPersonFaces();

public:
	Pf_FileHeader m_file_header;
	PERSONFACE_MAP m_person_faces;
	SFaceFt** m_facefts;
};

extern PersonFaceManager p_person_face_mgr;




#endif // _PERSON_MGT_H_
