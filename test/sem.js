var Cluster = require('cluster');
var Sem = require('../src/main.js');

function master(){
  mutex.lock();
  var slave = Cluster.fork();
  setInterval(function() {
    console.log(mutex.waiting());
  }, 1000);
  setTimeout(function() {
    console.log('master releasing');
    mutex.release();
  }, 5000);
  process.on('SIGINT', function() {
    console.log('exitting');
    mutex.delete();
    process.exit(0);
  });
};

function slave() {
  console.log('slave locking', mutex.locked());
  mutex.lock();
  console.log('slave running', mutex.locked());
};

var mutex = new Sem.Mutex(1234, Sem.Flags.IPC_CREAT);
Cluster.isMaster ? master() : slave();