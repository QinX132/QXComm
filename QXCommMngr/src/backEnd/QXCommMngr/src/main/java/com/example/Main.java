package com.example;

import java.io.IOException;
import java.util.Properties;
import org.apache.log4j.Logger;

public class Main {
    private int WorkerPort;
    private String RedisUrl;
    private String RedisUsrName;
    private String RedisPwd;
    private String MongoUrl;
    private String MongoUsrName;
    private String MongoPwd;
    private Logger Log;

    private void readConf() throws IOException {
        Properties properties = new Properties();
        properties.load(Main.class.getResourceAsStream("/config.properties"));

        this.MongoUrl = properties.getProperty("mongodb.url");
        this.MongoUsrName = properties.getProperty("mongodb.username");
        this.MongoPwd = properties.getProperty("mongodb.password");

        this.RedisUrl = properties.getProperty("redis.url");
        this.RedisUsrName = properties.getProperty("redis.username");
        this.RedisPwd = properties.getProperty("redis.password");

        try {
            this.WorkerPort = Integer.parseInt(properties.getProperty("WorkerPort"));
        } catch (NumberFormatException e) {
            throw new IOException("WorkerPort must be a valid integer value");
        }

        // 打印配置项
        Log.info("DB URL: " + this.MongoUrl);
        Log.info("DB Username: " + this.MongoUsrName);
        Log.info("DB Password: " + this.MongoPwd);
        Log.info("Redis URL: " + this.RedisUrl);
        Log.info("Redis Username: " + this.RedisUsrName);
        Log.info("Redis Password: " + this.RedisPwd);
        Log.info("WorkerPort: " + this.WorkerPort);
    }

    public static void main(String[] args) {
        Main main = new Main();
        //PropertyConfigurator.configure("log4j.properties");
        //PropertyConfigurator.configure(Main.class.getResourceAsStream("/log4j.properties"));
        main.Log = Logger.getLogger(Main.class);

        try {
            main.readConf();
        } catch (IOException e) {
            e.printStackTrace();
            System.exit(1); // 退出程序
        }

        MngrSvrWorker server = new MngrSvrWorker();
        server.startServer(main.WorkerPort, main.MongoUrl, main.RedisUrl);
    }
}
