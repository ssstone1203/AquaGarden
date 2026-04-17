package com.aquagarden.web;

import com.aquagarden.entity.User;
import com.aquagarden.repo.UserRepository;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@RestController
public class UserController {

    private final UserRepository userRepository;

    public UserController(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @GetMapping("/api/users")
    public List<Map<String, Object>> listUsers() {
        return userRepository.findAll().stream().map(this::toRow).collect(Collectors.toList());
    }

    private Map<String, Object> toRow(User u) {
        return Map.of(
                "id", u.getId(),
                "username", u.getUsername(),
                "email", u.getEmail() != null ? u.getEmail() : "",
                "role", u.getRole(),
                "created_at", u.getCreatedAt() != null ? u.getCreatedAt().toString() : ""
        );
    }
}
