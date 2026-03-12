const grpc = require('@grpc/grpc-js');
const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
const const_module = require('./const');

const message_proto = require('./proto');



async function GetVarifyCode(call, callback) {  //async关键字表示这是一个异步函数，可以使用await关键字等待Promise对象的结果
    console.log("email is ", call.request.email)
    try{
        uniqueId = uuidv4();
        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: '2789551808@qq.com',  // 发送方邮箱地址
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };

        let send_res = await emailModule.SendMail(mailOptions); // 发送邮件并等待结果(await关键字会等待Promise对象的结果)
        console.log("send res is ", send_res)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 


    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }

}

function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VarifyService.service, { GetVarifyCode: GetVarifyCode })
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()