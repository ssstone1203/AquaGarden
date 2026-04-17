package com.aquagarden.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

public record RegisterRequest(
        @NotBlank @Size(min = 3, max = 50) String username,
        @Size(max = 100) String email,
        @NotBlank @Size(min = 6, max = 100) String password
) {}
