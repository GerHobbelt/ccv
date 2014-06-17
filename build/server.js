var connect = require('connect');
connect.createServer(connect.static(__dirname + "/demo")).listen(8080);
