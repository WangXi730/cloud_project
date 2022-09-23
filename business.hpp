#include<iostream>
#include<string>
#include<sstream>
#include<boost/json.hpp>
#include<boost/date_time.hpp>
#include<unistd.h>
#include<mysql/mysql.h>
#include"bundle.h"
#include<fstream>
#include<vector>
#include<stack>

//业务处理模块
//一、用户账号模块需求
//    1 用户登录
//  	 1.1 注册用户信息（账号、密码、用户名）
//		 1.2 使用账号密码登录
//	  2 账号安全
//	  	 1.1 密保设置
//	  	 1.2 密码修改
//二、应用功能需求
//    1 数据处理
//    2 
//    


//1、账号业务

//用户类
class user;
//操作类
class user_factory;

//单例模式获得sql操作句柄，顺便提供工厂的操作
class project_db{
private:
	//考虑到这个几乎是必须的，所以采取饿汉模式
	project_db(){	
		//初始化sql
		handler  = mysql_init(NULL);
		//登录项目数据库
		mysql_real_connect(handler,"127.0.0.1","root","wangxi-730403730","project_of_cloud",0,NULL,0);
		//设置字符集
		mysql_set_character_set(handler,"utf8");
		//选择database
		mysql_select_db(handler,"project_of_cloud");
	}	
	MYSQL* handler;
	static project_db pdb;
	std::vector<MYSQL*> v;
public:
	//安全起见，上个锁
	static std::mutex _mutex;
	//析构函数释放资源，资源在对象中释放而非外部释放，是为了利用raii机制避免资源泄露
	~project_db(){
		for(MYSQL*& h:v){
			mysql_close(h);
		}
		mysql_close(handler);	
	}
	//向外提供原有的sql句柄
	static MYSQL* get_ptr(){
		return pdb.handler;
	}
	//向外提供接口获取新的sql操作句柄
	static MYSQL* get_new_ptr(){
		//初始化sql
		MYSQL* handler1  = mysql_init(NULL);
		//登录项目数据库
		mysql_real_connect(handler1,"127.0.0.1","root","wangxi-730403730","project_of_cloud",0,NULL,0);
		//设置字符集
		mysql_set_character_set(handler1,"utf8");
		//选择database
		mysql_select_db(handler1,"project_of_cloud");
		//添加到v中，统一释放
		pdb.v.push_back(handler1);
		return handler1;
	}
};

project_db project_db::pdb;
std::mutex project_db::_mutex;

class user{
private:
	//构造函数私有化，通过用户工厂实例化
	friend class user_factory;
	//每个user都需要操作数据库，所以都需要指针
	MYSQL* user_data;
	//每个user都有自己的id
	std::string user_id;
	//数据用json对象保存
	boost::json::object data;
	//构造方法(含解压)
	user(std::string id, MYSQL* ptr):user_id(id),user_data(ptr){
		//首先找到文件信息，解压文件，并将其转化为json对象，存放在data中
		//打开文件
		std::ifstream ifs("./server/"+id+".txt.lzip",std::ios::in);
		std::string s;
		//获取文件长度
		ifs.seekg(0,std::ios::end);
		size_t size = ifs.tellg();
		ifs.seekg(0,std::ios::beg);
		//读取文件
		s.resize(size);
		ifs.read(&s[0],size);
		//获取json对象
		if(s != ""){
			data = boost::json::parse(bundle::unpack(s)).as_object();
		}
		//关闭文件
		ifs.close();
	} 
public:
	//访问对应目录下的文件，并返回json对象
	boost::json::object visit(){
		//实例化一个json对象，作为返回值
		boost::json::object files;
		//查询
		std::string select_str = "select filename,size,date from table_"+user_id+";";
		mysql_query(user_data,select_str.c_str());
		MYSQL_RES* res = mysql_store_result(user_data);
		int num_row = mysql_num_rows(res);
		//把每个文件中的数据通过循环插入
		for(int i=0;i<num_row;++i){
			MYSQL_ROW f = mysql_fetch_row(res);
			//以文件名为键，其他数据为值，每个值是一个json对象
			files[f[0]] = {{"size",f[1]},{"date",f[2]}};
		}
		mysql_free_result(res);
		return files;		
	}
	//提供下载接口
	std::string download(std::string& filename){
	 	//获取数据
	 	std::stringstream res_packed;
	 	res_packed << data[filename].as_string();
		//解压
		std::string res = bundle::unpack(res_packed.str());
		//返回
		return std::string(res.begin()+1,res.end()-1);
	}
	//提供上传接口
	bool upload(std::string& filename, std::string& filedata){
		//查询是否存在这个文件，如果存在，则更新文件映射信息（即删除原有的信息，插入新的信息）
		std::cout<<user_id<<std::endl;
		std::cout<<filename<<std::endl;
		std::string select_str = "select filename from table_"+user_id+" where filename='"+filename+"';";
		mysql_query(user_data,select_str.c_str()); 
		MYSQL_RES* res = mysql_store_result(user_data);
		if(mysql_num_rows(res)!=0){
			//删除原有文件信息，并删除文件
			std::string delete_str = "delete from table_"+user_id+" where filename='"+filename+"';";
			mysql_query(user_data,delete_str.c_str()); 
		}
		//释放res
		mysql_free_result(res);
		//开始上传
		//获取文件数据，压缩并存放在val中
		std::string val = bundle::pack(bundle::LZIP,filedata);
		//将文件名：文件内容的压缩信息，放入data中
		data[filename] = val;
		//接下来，修改table的信息
		//获取时间，采用北京时间
		boost::posix_time::ptime pt = boost::posix_time::second_clock::universal_time() + boost::posix_time::hours(8);
		std::string file_up_date = boost::posix_time::to_iso_extended_string(pt);
		int i = 0;
		while(file_up_date[i]!='T'){
			++i;
		}
		file_up_date[i] = ' ';
		//插入新的文件信息
		std::string insert_str = "insert into table_"+user_id+"(filename,size,date) values('"+filename+"',"+std::to_string(filedata.size())+",'"+file_up_date+"');";
		mysql_query(user_data,insert_str.c_str()); 
		save();
		//插入完成，返回true
		return true;
	}
	void delete_file(std::string filename){	
		//查询是否存在这个文件，如果存在，删除
		std::string select_str = "select filename from table_"+user_id+" where filename='"+filename+"';";
		mysql_query(user_data,select_str.c_str()); 
		MYSQL_RES* res = mysql_store_result(user_data);
		if(mysql_num_rows(res)!=0){
			//删除原有文件信息，并删除文件
			std::string delete_str = "delete from table_"+user_id+" where filename='"+filename+"';";
			mysql_query(user_data,delete_str.c_str()); 
		}
		//删除文件信息
		data.erase(filename);
		//保存
		save();
	}
	~user(){
		save();
	}
	void save(){	
		//保存到文件
		std::ofstream ofs("./server/"+user_id+".txt.lzip",std::ios::out);
		ofs << data;
		ofs.close();
	}
};


class user_factory{
public:
	user_factory(){	
		db = project_db::get_ptr();
	}
	//登录接口，如果密码错误，则设置变量password为""
	user* login(std::string id, std::string& password){
		//首先使用sql查询账号是否存在	
		project_db::_mutex.lock();
		std::string select_str = "select password from user_id where id='"+id+"';";
		mysql_query(db,select_str.c_str());
		MYSQL_RES* res = mysql_store_result(db);
		project_db::_mutex.unlock();
		//账号不存在
		if(mysql_num_rows(res)==0){
			return nullptr;
		}
		MYSQL_ROW data = mysql_fetch_row(res);
		std::string real_password(data[0]);
		//密码不正确
		if(password!=real_password){
			password = "";
			return nullptr;
		}
		mysql_free_result(res);
		//走到这里说明账号存在且密码正确
		//接下来就算登录成功了，创建一个user对象
		MYSQL* ptr = project_db::get_new_ptr();
		user* ret = new user(id,ptr);	
		//记录在v中
		delete_v.push(ret);
		return ret;
	}
	//注册接口
	bool logon(std::string id, std::string password, std::string name){
		//在sql中添加账号信息
		std::string insert_str = "insert into user_id(name,id,password) values('"+name+"','"+id+"','"+password+"');";
		if(mysql_query(db,insert_str.c_str())!=0){
			return false;
		}
		//创建空压缩文件
		std::ofstream ofs("./server/"+id+".txt.lzip",std::ios::out);
		ofs.close();
		//添加一个表，用来记录这个人的文件信息
		insert_str = "create table table_"+id+"(filename char(50), size int, date datetime);";
		mysql_query(db,insert_str.c_str());
		return true;
	}
	//保存接口(保存需要删除现有的所有指针)
	void save(){	
		//释放所有提供的指针
		while(!delete_v.empty()){
			user* du = delete_v.top();
			delete_v.pop();
			delete du;
		}
	}
	~user_factory(){
		//保存
		save();
	}
private:
	MYSQL* db;
	std::stack<user*> delete_v;
};

