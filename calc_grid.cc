#include "node.h"
#include <stdio.h>
#include <map>
#include <vector>
#include<string>
#include<sstream>
#include<uv.h>
#include<regex>
using namespace v8;
using namespace std;
class ColumnValue {
public:
	string name,cong_thuc;
	double value = 0;
	ColumnValue(string name, string cong_thuc="", double value=0) {
		this->name = name;
		this->cong_thuc = cong_thuc;
		this->value = value;
	}
	ColumnValue(){}
};
class Row {
public:
	string ma_so,cong_thuc;
	map<string,ColumnValue> gia_tris;
	Row(string ma_so,string cong_thuc,map<string,double> col_gia_tris) {
		this->ma_so = ma_so;
		this->cong_thuc = cong_thuc;
		for (map<string,double>::iterator v = col_gia_tris.begin(); v != col_gia_tris.end(); v++) {
			ColumnValue gia_tri(v->first, cong_thuc, v->second);
			this->gia_tris[v->first] = gia_tri;
		}
	}
	Row(){}
	static void split(string str, char delim,vector<string> &v) {
		stringstream st(str);
		string item;
		while (getline(st, item, delim)) {
			v.push_back(item);
		}
	}
	static string double_to_string(double &d) {
		stringstream s;
		s << d;
		string v = s.str();
		return v;
	}
	static void replaceAll(string &ctx, string search, string rep) {
		rsize_t pos = 0;
		while ((pos = ctx.find(search, pos)) != std::string::npos) {
			ctx.replace(pos, search.length(), rep);
			pos += rep.length();
		}
	}
	bool prepairdCong_thuc( map<string, Row> rows) {
		bool kq = false;
		if (this->cong_thuc!="") {
			for (map<string, Row>::iterator r = rows.begin(); r != rows.end(); r++) {
				for (map<string, ColumnValue>::iterator gia_tr = this->gia_tris.begin(); gia_tr != this->gia_tris.end(); gia_tr++) {
					if (gia_tr->second.cong_thuc != "") {
						string old_cong_thuc = gia_tr->second.cong_thuc;
						//replace value
						if (r->second.gia_tris[gia_tr->first].cong_thuc == "") {
							replaceAll(gia_tr->second.cong_thuc, "[" + r->second.ma_so + "]", double_to_string(r->second.gia_tris[gia_tr->first].value));
						}
						else {
							replaceAll(gia_tr->second.cong_thuc, "[" + r->second.ma_so + "]", "(" + r->second.gia_tris[gia_tr->first].cong_thuc + ")");
						}
						//replace formula
						if (old_cong_thuc != gia_tr->second.cong_thuc) {
							
							kq = true;
						}
						
					}
				}
			}
		}
		return kq;
		
	}
};
struct AsyncData {
	Persistent<Value> data;
	map<string,Row> rows;
	Persistent<Function> callback;
};

void worker(uv_work_t *req) {
	auto asyncData = reinterpret_cast<AsyncData *>(req->data);
	//prepair formula
	bool cont = true;
	int n = 0;
	int max = asyncData->rows.size() * 100;
	while (cont && n< max) {
		cont = false;
		n++;
		for (map<string, Row>::iterator r = asyncData->rows.begin(); r != asyncData->rows.end(); r++) {
			if (r->second.cong_thuc != "") {
				if (r->second.prepairdCong_thuc(asyncData->rows)) {
					cont = true;
				}
				//
				for (map<string, ColumnValue>::iterator it = r->second.gia_tris.begin(); it != r->second.gia_tris.end(); it++) {
					ColumnValue *col = &(it->second);
					int v_t = col->cong_thuc.find("[", 0);
					if (v_t < 0) {
						r->second.cong_thuc = "";
					}
				}

			}
		}
	}
	//fix formula error
	std::regex reg("(\\[)([a-zA-Z0-9_]+)(\\])");
	
	for (map<string, Row>::iterator row = asyncData->rows.begin(); row != asyncData->rows.end(); row++) {
		for (map<string, ColumnValue>::iterator it = row->second.gia_tris.begin(); it != row->second.gia_tris.end(); it++) {
			if (it->second.cong_thuc.find("[", 0) >= 0) {
				it->second.cong_thuc = std::regex_replace(it->second.cong_thuc, reg, "0");
			}
		}
	}
	//result
	req->data = asyncData;
	
}
void callCallback(uv_work_t *req,int status) {
	Isolate *isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
	auto asyncData = reinterpret_cast<AsyncData *>(req->data);
	
	Local<Value> data = Local<Value>::New(isolate, asyncData->data);
	
	Local<Object> obj = data->ToObject();
	Local<String> field_ma_so = String::NewFromUtf8(isolate, "ma_so");
	//fill value
	for (int i = 0; i < asyncData->rows.size(); i++) {
		Local<Object> item = obj->Get(i)->ToObject();
		String::Utf8Value o_ma_so(item->Get(field_ma_so)->ToString());
		string ma_so = *o_ma_so;
		for (map<string, ColumnValue>::iterator it = asyncData->rows[ma_so].gia_tris.begin(); it != asyncData->rows[ma_so].gia_tris.end(); it++) {
			double value = it->second.value;
			
			if (it->second.cong_thuc != "") {
				TryCatch trycatch;
				Local<Script> script = Script::Compile(String::NewFromUtf8(isolate, it->second.cong_thuc.c_str()));

				Local<Value> kq = script->Run();

				if (kq.IsEmpty()) {
					Local<Value> exception = trycatch.Exception();
					String::Utf8Value exception_str(exception);
					printf("Exception: %s\n ", *exception_str);
					value = 0;
				}
				else {
					if (kq->IsNumber()) {
						value = kq->NumberValue();
						if (value == INFINITY) {
							value = 0;
						}
					}
					else {
						value = 0;
					}

				}
				
			}
			
			item->Set(String::NewFromUtf8(isolate, it->first.c_str()), Number::New(isolate, value));
		}

	}
	
	//call callback
	data = Local<Value>::New(isolate, obj);
	Handle<Value> arr_value[] = { data };
	Local<Function> callback = Local<Function>::New(isolate, asyncData->callback);
	callback->Call(isolate->GetCurrentContext()->Global(),1,arr_value);
	delete asyncData;
	delete req;

}
void calcGrid2(const FunctionCallbackInfo<Value>& args) {

	Isolate *isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);


	if (args.Length() < 3) {
		isolate->ThrowException(String::NewFromUtf8(isolate, "Miss argment"));
		return;
	}
	if (!args[0]->IsArray()) {
		isolate->ThrowException(String::NewFromUtf8(isolate, "bad argument"));
		return;
	}

	if (!args[2]->IsFunction()) {
		isolate->ThrowException(String::NewFromUtf8(isolate, "Miss callback"));
		return;
	}
	//get rows
	Handle<Value> l_data = args[0];
	Handle<Object> obj = l_data->ToObject();
	map<string, Row> rows;
	Local<String> field_cong_thuc = String::NewFromUtf8(isolate, "cong_thuc");
	Local<String> field_ma_so = String::NewFromUtf8(isolate, "ma_so");

	vector<string> gia_tris;
	if (args[1]->IsString()) {
		String::Utf8Value col_gia_tris(args[1]->ToString());
		Row::split(*col_gia_tris, ',', gia_tris);
	}
	else {
		Row::split("gia_tri", ',', gia_tris);
	}

	int length = obj->Get(String::NewFromUtf8(isolate, "length"))->IntegerValue();
	for (int i = 0; i < length; i++) {
		Local<Object> item = obj->Get(i)->ToObject();

		String::Utf8Value o_ma_so(item->Get(field_ma_so)->ToString());
		string ma_so = *o_ma_so;

		string cong_thuc = "";
		if (item->Get(field_cong_thuc)->IsString()) {
			String::Utf8Value o_cong_thuc(item->Get(field_cong_thuc)->ToString());
			cong_thuc = *o_cong_thuc;
		}
		map<string, double> col_gia_tris;
		for (vector<string>::iterator it = gia_tris.begin(); it != gia_tris.end(); it++) {
			double gia_tri = 0;
			Local<Value> lv = item->Get(String::NewFromUtf8(isolate, (*it).c_str()));
			if (lv->IsNumber()) {
				gia_tri = lv->NumberValue();
			}
			col_gia_tris[*it] = gia_tri;
		}


		Row r(ma_so, cong_thuc, col_gia_tris);
		rows[ma_so] = r;
	}
	//get callback function
	Local<Function> l_callback = Local<Function>::Cast(args[2]);
	//create async data
	AsyncData *asyncData = new AsyncData;
	asyncData->callback.Reset(isolate, l_callback);
	asyncData->data.Reset(isolate, l_data);
	asyncData->rows = rows;
	//run async
	uv_work_t *req = new uv_work_t;
	req->data = asyncData;
	uv_queue_work(uv_default_loop(), req, worker, callCallback);
}
void init(Handle<Object> exports){
	NODE_SET_METHOD(exports,"calcGrid",calcGrid2);
}
NODE_MODULE(stp,init)