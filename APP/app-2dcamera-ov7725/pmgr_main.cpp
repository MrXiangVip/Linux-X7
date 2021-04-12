#include "pmgr.h"

int main(int argc, char **argv) {
	cout << sizeof(Person) << endl;
	int person_id = 1;
	if (argc < 2) {
		cout<<"Usage: pmgmt [l|s|d|r|f] [PERSON_ID]"<< endl;
		return -1;
	}
	if(argc > 2) {
		sscanf(argv[2], "%d", &person_id);
	}

	if (strcmp(argv[1], "l") == 0) {
		p_person_mgr.loadPersonManager();
		cout << "=======" <<  endl;
		cout << p_person_mgr.m_person.size()<< endl;
		cout << p_person_mgr.m_file_header << endl;
		map<int, Person>::iterator it;
		for(it = p_person_mgr.m_person.begin(); it != p_person_mgr.m_person.end(); ++it) {
			Person person = it->second;
			cout << person << endl;
		}
		cout << p_person_mgr.m_person.size() << endl;
	} else if(strcmp(argv[1], "s") == 0) {
		Person p = genPerson(1,1,1,"u1","p1","c1","t1");
		p_person_mgr.registerUser(p);
		p = genPerson(2,2,2,"u2","p2","c2","t2");
		p_person_mgr.registerUser(p);
	} else if (strcmp(argv[1], "d") == 0) {
		p_person_mgr.loadPersonManager();
		bool res = p_person_mgr.deleteUserById(person_id);
	} else if (strcmp(argv[1], "r") == 0) {
		p_person_mgr.loadPersonManager();
		bool res = p_person_mgr.isUserExisted(person_id);
	} else if (strcmp(argv[1], "f") == 0) {
		p_person_mgr.loadPersonManager();
	} else {
		cout<<"Usage: pmgmt [l|s|d|r] [PERSON_ID]"<< endl;
		return -1;
	}
	return 0;
}
