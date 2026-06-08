const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { v4: uuidv4 } = require('uuid');
const nodemailer = require('nodemailer');

const PROTO_PATH = __dirname + '/message.proto';
const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
  keepCase: true,
  longs: String,
  enums: String,
  defaults: true,
  oneofs: true,
});
const proto = grpc.loadPackageDefinition(packageDefinition).message;

const transporter = nodemailer.createTransport({
  host: process.env.SMTP_HOST || 'smtp.ethereal.email',
  port: parseInt(process.env.SMTP_PORT || '587'),
  secure: false,
  auth: {
    user: process.env.SMTP_USER || '',
    pass: process.env.SMTP_PASS || '',
  },
});

function getVerifyCode(call, callback) {
  const email = call.request.email;
  const code = uuidv4().substring(0, 6);

  if (!email) {
    return callback(null, { error: 1, email: '', code: '' });
  }

  const mailOptions = {
    from: process.env.SMTP_FROM || 'noreply@example.com',
    to: email,
    subject: 'Verification Code',
    text: `Your verification code is: ${code}`,
  };

  transporter.sendMail(mailOptions, (err) => {
    if (err) {
      console.error('Failed to send email:', err.message);
      return callback(null, { error: 2, email, code: '' });
    }
    console.log(`Verification code sent to ${email}: ${code}`);
    callback(null, { error: 0, email, code });
  });
}

function main() {
  const server = new grpc.Server();
  server.addService(proto.VerifyService.service, { getVerifyCode });
  const port = process.env.PORT || '50051';
  server.bindAsync(
    `0.0.0.0:${port}`,
    grpc.ServerCredentials.createInsecure(),
    (err, port) => {
      if (err) {
        console.error(err);
        return;
      }
      console.log(`gRPC server running on port ${port}`);
    }
  );
}

main();
