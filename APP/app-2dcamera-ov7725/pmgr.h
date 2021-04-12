#ifndef _PERSON_MGT_H_
#define _PERSON_MGT_H_

#include <iostream>
#include <map>
#include <stdio.h>
#include <memory.h>

using namespace::std;

#define PERSON_MAX_COUNT 200
#define PERSON_FEATURE_NUM 5
#define FACE_FEATURE_LEN 256
#define PERSON_DB_FILE "person.db"

#define NAME_BUF_SIZE 31
#define PWD_BUF_SIZE 15
#define CARD_NO_SIZE 31
#define USER_LOGO_LEN 32 * 48 * 3 + 52

#define VERSION_V1 1

class Pm_FileHeader {
	public:
	int headerSize;
	int maxCount;
	int currentCount;
	int version;
	int reserved;
	Pm_FileHeader() {
		headerSize = 20;
		maxCount = PERSON_MAX_COUNT;
		currentCount = 0;
		version = 1;
		reserved = 0;
	}
	friend ostream& operator<<(ostream & os,const Pm_FileHeader &fileHeader) {
		os << "Pm_FileHeader:" << endl;
		os << "\theaderSize: "<< fileHeader.headerSize << endl;
		os << "\tmaxCount: "<< fileHeader.headerSize << endl;
		os << "\tcurrentCount: "<< fileHeader.currentCount<< endl;
		os << "\tversion: "<< fileHeader.version << endl;
		os << "\treserved: "<< fileHeader.reserved << endl;
		return os;
	}
};

typedef enum EPersonType {
	SUPERADMIN,
	ADMIN,
	USER,
} PersonType;

#if 1
class Person {
	public:
	int personId;
	int personType;	// 0 superadmin 1 admin 2 User
	int securityType; // &1 password &2 face &4 card
	char userName[NAME_BUF_SIZE + 1]; 
	char userPwd[PWD_BUF_SIZE + 1]; 
	char cardNo[CARD_NO_SIZE + 1]; 
	char thumbnail[USER_LOGO_LEN];
	Person() {
		personId = -1;
		personType = 2;
		securityType = 7;
		memset(userName, '\0', sizeof(userName));
		strcpy(userName, "user");
		memset(userPwd, '\0', sizeof(userPwd));
		strcpy(userPwd, "****");
		memset(cardNo, '\0', sizeof(cardNo));
		strcpy(cardNo, "cc");
		memset(thumbnail, '\0', sizeof(thumbnail));
		strcpy(thumbnail, "XXXX");
	}
	friend ostream& operator<<(ostream & os,const Person &person) {
		os << "Person:" << endl;
		os << "\tpersonId: "<< person.personId<< endl;
		os << "\tpersonType: "<< person.personType<< endl;
		os << "\tsecurityType: "<< person.securityType<< endl;
		os << "\tuserName: "<< person.userName<< endl;
		os << "\tuserPwd: "<< person.userPwd<< endl;
		os << "\tcardNo: "<< person.cardNo<< endl;
		return os;
	}
};
#endif

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

typedef map<int, Person> PERSON_MAP;

class PersonManager {
public:
public:
	bool loadPersonManager(const char* file_name);
	bool savePersonManager(const char* file_name);
	bool loadPersonManager();
	bool savePersonManager();

	bool isUserExisted(int person_id);
	bool isUserExisted(const char* user_name);
	bool registerUser(Person person);
	bool updateUser(Person person);
	bool deleteUserById(int person_id);

	int getMaxPersonId();

public:
	Pm_FileHeader m_file_header;
	PERSON_MAP m_person;
};

extern PersonManager p_person_mgr;
Person genPerson(long long person_id, int person_type, const char* thumbnail) ;

#endif // _PERSON_MGT_H_
