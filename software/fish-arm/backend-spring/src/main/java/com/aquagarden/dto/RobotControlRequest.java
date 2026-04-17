package com.aquagarden.dto;

import jakarta.validation.constraints.NotBlank;

public record RobotControlRequest(@NotBlank String direction) {}
