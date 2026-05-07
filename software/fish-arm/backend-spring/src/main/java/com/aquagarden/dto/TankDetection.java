package com.aquagarden.dto;

/**
 * 鱼缸目标检测框（坐标为相对于图像宽高的 0~1 归一化，原点在左上）。
 */
public record TankDetection(String label, double x, double y, double width, double height, double score) {}
