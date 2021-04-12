#include "pfmgr.h"

SFaceFt* genFaceFt(int v, const char *pLocal) {
	SFaceFt *face = new SFaceFt();
	face->nFaceCount = 1;
	face->nVersion = v;
	int nLocalFtLen = 256;
	byte *pbLocalFt = new byte[nLocalFtLen];
	// const char *pLocal = "abcdefgfdkalsfdlasf;dasl;fdsak;fdsak;fdkas;fkdsa;";
	memcpy(pbLocalFt, pLocal, strlen(pLocal));
	face->pbLocalFt = pbLocalFt;
	face->nLocalFtLen = nLocalFtLen;
	face->nTimeUpdate = 20200706;
	return face;
}

int main(int argc, char **argv) 
{
	cout << sizeof(PersonFace) << endl;
	int person_id = 1;
	if (argc < 2) {
		cout<<"Usage: pmgmt [l|s|d|r|f] [PERSON_ID]"<< endl;
		return -1;
	}
	if(argc > 2) {
		sscanf(argv[2], "%d", &person_id);
	}

	if (strcmp(argv[1], "l") == 0) {
			p_person_face_mgr.loadPersonFaceMgr();
			cout << "=======" <<  endl;
			cout << p_person_face_mgr.m_person_faces.size()<< endl;
			cout << p_person_face_mgr.m_file_header << endl;
			map<int, PersonFace>::iterator it;
			for(it = p_person_face_mgr.m_person_faces.begin(); it != p_person_face_mgr.m_person_faces.end(); ++it) {
			PersonFace person = it->second;
			cout << person << endl;
			}
			cout << p_person_face_mgr.m_person_faces.size() << endl;
	} else if(strcmp(argv[1], "s") == 0) {
		uUID uu_id;
		SFaceFt* face = genFaceFt(100, "fdaf;kldaslfdlksafjdl;afjdka;re3|");
		p_person_face_mgr.registerFaceFt(1, face, &uu_id);
		face = genFaceFt(101, "lfdavcxzvm,czxnvmczxnvmn32423fda");
		p_person_face_mgr.registerFaceFt(2, face, &uu_id);
		face = genFaceFt(202, "vczx.reo423fldac./m,;xk:jkfa");
		p_person_face_mgr.registerFaceFt(3, face , &uu_id);
	} else if (strcmp(argv[1], "d") == 0) {
		p_person_face_mgr.loadPersonFaceMgr();
		bool res = p_person_face_mgr.deletePersonById(person_id);
	} else if (strcmp(argv[1], "r") == 0) {
		p_person_face_mgr.loadPersonFaceMgr();
		bool res = p_person_face_mgr.hasPersonFace(person_id);
	} else if (strcmp(argv[1], "f") == 0) {
		p_person_face_mgr.loadPersonFaceMgr();
		SFaceFt** faces = p_person_face_mgr.getPersonFaces();
		for (int i = 0; i < p_person_face_mgr.m_person_faces.size(); i++) {
			SFaceFt* face = *faces++;
			if (face != NULL && face->pbLocalFt != NULL) {
				cout << "\tpbLocalFt: "<< (char*)(face->pbLocalFt)<< endl;
			}
		}
	} else {
		cout<<"Usage: pmgmt [l|s|d|r] [PERSON_ID]"<< endl;
		return -1;
	}
	return 0;
}
