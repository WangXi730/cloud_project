#include<iostream>
#include<string>
#include<boost/json.hpp>
#include"httplib.h"
//客户端

//Raii机制的安全登录类
class this_login{
public:
	this_login(httplib::MultipartFormDataItems i, httplib::Client& c):items(i),cli(c)
	{}
	~this_login(){
		//退出登录
		cli.Post("/unlogin",items);
	}
private:
	httplib::MultipartFormDataItems items;
	httplib::Client& cli;
};

int main(){
	//做一个简单的界面
	int choose = -1;
	httplib::Client cli("101.34.32.158",9090);
	while(choose != 0){
		choose = 0;
		std::cout << "1、注册" << std::endl;
		std::cout << "2、登录" << std::endl; 
		std::cout << "0、退出（默认）" << std::endl;
		std::cin >> choose;
		if(choose == 1){
			std::cout << "请输入用户名：" << std::endl;
			std::string name;
			std::cin >> name;
			if(name.size()>30){
				std::cout << "用户名长度不超过30" << std::endl;
				continue;
			}		
			std::cout << "请输入密码：(8~15位)" << std::endl;
			std::string password;
			std::cin >> password;
			if(password.size()<8||password.size()>15){
				std::cout << "密码长度不合格" << std::endl;
				continue;
			}
			//构造注册信息
			boost::json::object data;
			data["name"] = name;
			data["password"] = password;
			//序列化
			std::string sdata = boost::json::serialize(data);
			//发送注册请求
			httplib::MultipartFormDataItems items;
			httplib::MultipartFormData item;
			item.name = "logondata";
			item.filename = "data";
			item.content = sdata;
			item.content_type = "application/octet-stream";
			items.push_back(item);
			auto res = cli.Post("/logon",items);
			if(res->status == 200){
				if(res->body != "Refused"){
					std::cout << "注册成功！\n您的id为：\n"<< res->body << std::endl;
					std::cout << "请牢记！" << std::endl;
				}
				else{
					std::cout << "请求被拒绝！" << std::endl;
				}
			}
			else{
				std::cout << "请求失败" << std::endl;
			}
		}
		else if(choose == 2){
			//登录
			std::cout << "请输入用户id：（10位）" << std::endl;
			std::string id;
			std::cin >> id;
			if(id.size()!=10){
				std::cout << "请输入正确长度的id" << std::endl;
				continue;
			}
			
			std::cout << "请输入密码：(8~15位)" << std::endl;
			std::string password;
			std::cin >> password;
			if(password.size()<8||password.size()>15){
				std::cout << "密码长度不合格" << std::endl;
				continue;
			}
			//记录信息
			boost::json::object obj;
			obj["id"] = id;
			obj["password"] = password;
			//上传信息	
			httplib::MultipartFormDataItems items;
			httplib::MultipartFormData item;
			item.name = "logindata";
			item.filename = "data";
			item.content = boost::json::serialize(obj);
			item.content_type = "application/octet-stream";
			items.push_back(item);
			auto res = cli.Post("/login",items);
			if(res->body == "password error!"||res->body == "id inexist"){
				std::cout << "登录失败！" << std::endl;
				if(res->body == "password error!"){
					std::cout << "失败原因：密码错误！" << std::endl;
				}
				else{
					std::cout << "失败原因：账号不存在！" << std::endl;
				}
				continue;
			}
			//登录成功，提示并打印文件
			std::cout << "登录成功！" << std::endl;
			//安全起见，使用raii机制，让程序结束后，告诉服务器退出
			httplib::MultipartFormDataItems items2;
            httplib::MultipartFormData item2;
            item2.name = "end";
            item2.filename = "data";
            item2.content = boost::json::serialize(obj);
            item2.content_type = "application/octet-stream";
            items2.push_back(item2);
			//打印登录后的选项
			int choose2 = -1;
			while(choose2 != 0){
				boost::json::object obj = boost::json::parse(res->body).as_object();
				std::cout << "账号保存的文件信息如下："<< std::endl;
				if(obj.empty()){
					std::cout << "empty" << std::endl;
				}
				else {
					for(auto &e:obj){
						std::cout << e.key() << " ：" << e.value() << std::endl;
					}
				}
				//打印登录成功后操作选项
				std::cout << "请选择：" << std::endl;
				std::cout << "1、上传文件" << std::endl;
				std::cout << "2、删除文件" << std::endl;
				std::cout << "3、下载文件" << std::endl;
				std::cout << "0、退出登录状态(默认)" << std::endl;
				choose2 = 0;
				std::cin >> choose2;
				if(choose2==0){
          			cli.Post("/unlogin",items2);
				}
				else if(choose2==1){
					std::cout << "上传的文件名：" ;
					std::string filename;
					std::cin >> filename;
					//打开文件
					std::ifstream ifs(filename,std::ios::in);
					ifs.seekg (0, ifs.end);
				    int length = ifs.tellg();
				    ifs.seekg (0, ifs.beg);
					std::string filedata;
					filedata.resize(length);
					ifs.read(&filedata[0],length);
					//上传操作
					httplib::MultipartFormDataItems items3;
					httplib::MultipartFormData item3;
					item3.name = "updatedata";
					item3.filename = id;
					boost::json::object file_data;
					file_data["filedata"] = filedata;
					file_data["filename"] = filename;
					item3.content = boost::json::serialize(file_data);
					item3.content_type = "application/octet-stream";
					items3.push_back(item3);
					auto ret = cli.Post("/upload",items3);
					if(ret->status == 200){
						std::cout << "上传成功" << std::endl;
						//更新数据
						res->body = ret->body;
					}
					else {
						std::cout << "上传失败" << std::endl;
					}
				}
				else if(choose2==2){
					std::cout << "需要删除的文件名称：";
					std::string filename;
					std::cin >> filename;
					//发送删除请求
					httplib::MultipartFormDataItems items3;
					httplib::MultipartFormData item3;
					item3.name = "deletedata";
					item3.filename = id;
					item3.content = filename;
					item3.content_type = "application/octet-stream";
					items3.push_back(item3);
					auto ret = cli.Post("/delete",items3);
					if(ret->status == 200){
						std::cout << "删除成功！" << std::endl;
						res->body = ret->body;
					}
					else{
						std::cout << "删除失败！" << std::endl;
					}
				}
				else if(choose2==3){
					std::cout << "需要下载的文件名：";
					std::string filename;
					std::cin >> filename;
					std::cout << "需要下载的文件路径：";
					std::string fileaddr;
					std::cin >> fileaddr;
					//发送下载请求
					httplib::MultipartFormDataItems items3;
					httplib::MultipartFormData item3;
					item3.name = "downloaddata";
					item3.filename = id;
					item3.content = filename;
					item3.content_type = "application/octet-stream";
					items3.push_back(item3);
					auto ret = cli.Post("/download",items3);
					//下载的文件内容存放在body中，创建文件
					std::ofstream ofs(fileaddr+filename,std::ios::out);
					//填入内容
					std::string filedata = ret->body;
					std::cout << filedata<<std::endl;
					ofs.write(&filedata[0],filedata.size());
					//关闭文件
					ofs.close();
				}
			}
		}
	}
	return 0;
}
