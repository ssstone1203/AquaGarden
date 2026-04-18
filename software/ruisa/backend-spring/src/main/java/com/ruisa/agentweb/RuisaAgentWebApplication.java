package com.ruisa.agentweb;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.EnableConfigurationProperties;

import com.ruisa.agentweb.config.AgentProxyProperties;

@SpringBootApplication //@SpringBootApplication 自动扫描并注册所有带@component等注解的类为bean
@EnableConfigurationProperties(AgentProxyProperties.class)//将agentproxyproperties 注册为bean
public class RuisaAgentWebApplication {

    public static void main(String[] args) {
        SpringApplication.run(RuisaAgentWebApplication.class, args);//启动spring容器，创建和管理所有的bean
    }
}
