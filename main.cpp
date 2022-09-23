#include<iostream>
#include<boost/json.hpp>
#include<boost/filesystem.hpp>
#include<map>
#include<string>
#include"httplib.h"
#include"business.hpp"

using namespace httplib;
//工厂
user_factory fac;

//激活账号名单
std::map<std::string, user*> mp;

//登录
void Login(const Request& req, Response& rep){
	//req 是请求信息，rep是需要填写的返回信息
	if(req.has_file("logindata")){
		rep.status = 200;
		httplib::MultipartFormData item = req.get_file_value("logindata");
		std::string data = item.content;
		boost::json::object obj;
		try{
			obj = boost::json::parse(data).as_object();
		}
		catch(...){
			//失败不做处理
		}
		std::stringstream id;
		std::stringstream password;
		id << obj["id"].as_string();
		password << obj["password"].as_string();
		std::string _id = id.str();
		//去掉双引号
		std::string _p = password.str();
		std::string __id(_id.begin()+1,_id.end()-1);
		std::string __p(_p.begin()+1,_p.end()-1);
		user* u = fac.login(__id, __p);
		//添加进入账号激活名单
		if(u!=nullptr){
			mp[__id] = u;
		}
		if(u==nullptr){
			//登录失败
			if(obj["password"]==""){
				//密码不正确
				rep.body = "password error!";
			}
			else{
				//账号不存在
				rep.body = "id inexist";
			}
			return;
		}
		//登录成功
		//获取文件信息，并返回
		boost::json::object info = u->visit();
		rep.body = boost::json::serialize(info);
		return;
	}
}

//全局变量控制注册人数
int count = 0;
std::mutex count_mutex;
std::string today;
	
void Logon(const Request& req, Response& rep){
	//注册	
	if(req.has_file("logondata")){
		//能进入这里，说明正常
		rep.status = 200;
		//分配id
		//10位id，年+月+日+号码，号码为2位，即，理论上每天允许100个人注册id，但如果上一个人注册的日期，没注册满，可以保留到下一个注册的日期（最多保留一天）
		std::stringstream id;
		if(count == 0){
			//获取今天的日期（北京时间）
			boost::posix_time::ptime pt = boost::posix_time::second_clock::universal_time() + boost::posix_time::hours(8);
			std::stringstream ss;
			ss << pt.date().year();
			std::string tmp;
			if(pt.date().month()<10)
				tmp = "0";
			ss << tmp << (int)pt.date().month();
			if(pt.date().day() >= 10)
				tmp = "";
			ss << tmp << pt.date().day();
			//比较时间是否相同
			if(ss.str()==today){
				//今天已经没有注册账号的名额了
				rep.body = "Refused";
				return;
			}
			else{
				//切换时间为今天
				today = ss.str();
			}
		}
		if(count < 99){
			count_mutex.lock();
			if(count < 99){
				//小于99，则获取id
				std::string tmp = "0";
				if(count >= 10){
					tmp = "";
				}
				id << today << tmp << count;
				count += 1;
			}
			count_mutex.unlock();
		}	
		else{
			//今天已经注册满了
			count_mutex.lock();
			count = 0;
			count_mutex.unlock();
			rep.body = "Refused";
			return;
		}
		//在系统中注册信息
		//获取密码和用户名信息	
		httplib::MultipartFormData item = req.get_file_value("logondata");
		std::string data = item.content;
		boost::json::object obj;
		try{
			obj = boost::json::parse(data).as_object();
		}
		catch(...){
			//失败不做处理
		}
		std::stringstream ss;
		std::string password;
		std::string name;
		ss << obj["password"];
		ss >> password;
		ss.clear();
		ss << obj["name"];	
		ss >> name;
		std::string _id = id.str();
		//去除双引号
		std::string _password(password.begin()+1,password.end()-1);
		std::string _name(name.begin()+1,name.end()-1);
		if(!fac.logon(_id,_password,_name)){
			std::cout << "注册失败！" <<std::endl;
		}
		//返回账号
		rep.body = id.str();
		return;
	}
}

void unlogin(const Request& req, Response& rep){
	//退出登录
	if(req.has_file("end")){
		//获取账号
		rep.status = 200;
        httplib::MultipartFormData item = req.get_file_value("end");
        std::string data = item.content;
        boost::json::object obj;
        try{
            obj = boost::json::parse(data).as_object();
        }
        catch(...){
            //失败不做处理
        }
		//释放mp种的登录信息
		std::stringstream id;
		id << obj["id"];
		//去双引号:
		std::string _id(id.str().begin()+1,id.str().end()-1);
		mp.erase(_id);
		//返回

		return;
	}
}

void upload(const Request& req, Response& rep){
	//添加文件
	if(req.has_file("updatedata")){
        httplib::MultipartFormData item = req.get_file_value("updatedata");
        std::string file_data = item.content;
		boost::json::object obj = boost::json::parse(file_data).as_object();
		std::stringstream ss;
		ss << obj["filename"];
		std::string filename;
		ss >> filename;
		ss.clear();
		ss << obj["filedata"];
		std::string filedata;
		ss >> filedata;
		//存储
		//获取id
		std::string id = item.filename;
		//根据id查找user
		user* u = mp[id];
		//去括号
		std::string _filename(filename.begin()+1,filename.end()-1);
		std::string _filedata(filedata.begin()+1,filedata.end()-1);
		//存储操作
		if(u->upload(_filename,_filedata)){
			rep.status = 200;//代表操作成功
			//返回访问值
			obj = u->visit();
			//序列化返回
			rep.body = boost::json::serialize(obj);
		}
	}
}

void download(const Request& req, Response& rep){
	//下载文件
	if(req.has_file("downloaddata")){
		//查找账号信息
        httplib::MultipartFormData item = req.get_file_value("downloaddata");
        std::string filename = item.content;
		//存储
		//获取id
		std::string id = item.filename;
		//根据id查找user
		user* u = mp[id];
		//下载操作
		rep.body = u->download(filename);
		rep.status = 200;//代表操作成功
	}
	//返回
}
void delete_file(const Request& req, Response& rep){
	//删除文件
	if(req.has_file("deletedata")){
		//查找账号信息
        httplib::MultipartFormData item = req.get_file_value("deletedata");
        std::string filename = item.content;
		//存储
		//获取id
		std::string id = item.filename;
		//根据id查找user
		user* u = mp[id];
		//删除操作
		u->delete_file(filename);	
		rep.status = 200;//代表操作成功
		//返回访问值
		boost::json::object obj = u->visit();
		//序列化返回
		rep.body = boost::json::serialize(obj);
	}
	//返回
}

//server端
int main(){
	httplib::Server svr;
	svr.Post("/logon",Logon);
	svr.Post("/unlogin",unlogin);
	svr.Post("/login",Login);
	svr.Post("/upload",upload);
	svr.Post("/delete",delete_file);	
	svr.Post("/download",download);
	svr.listen("0.0.0.0",9090);
	return 0;
}
