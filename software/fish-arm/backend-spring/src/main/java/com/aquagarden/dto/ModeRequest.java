package com.aquagarden.dto;

import jakarta.validation.constraints.NotBlank;

public record ModeRequest(@NotBlank String mode) {}
