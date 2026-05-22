package com.aquagarden.dto;

import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;
import jakarta.validation.constraints.Size;

import java.util.List;

public record AiChatRequest(
        @NotBlank
        @Size(max = 600)
        String message,

        @Size(max = 12)
        List<@Valid ChatMessage> history
) {
    public record ChatMessage(
            @NotBlank
            @Pattern(regexp = "user|assistant")
            String role,

            @NotBlank
            @Size(max = 6000)
            String content
    ) {
    }
}
