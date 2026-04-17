package com.aquagarden.service;

import com.aquagarden.dto.LoginRequest;
import com.aquagarden.dto.RegisterRequest;
import com.aquagarden.dto.TokenResponse;
import com.aquagarden.entity.User;
import com.aquagarden.repo.UserRepository;
import org.springframework.http.HttpStatus;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.web.server.ResponseStatusException;

import java.util.Map;
import java.util.regex.Pattern;

@Service
public class AuthService {

    private static final Pattern EMAIL = Pattern.compile("^[\\w.+-]+@[\\w.-]+\\.[a-zA-Z]{2,}$");

    private final UserRepository userRepository;
    private final PasswordEncoder passwordEncoder;
    private final JwtService jwtService;
    private final int expirationMinutes;

    public AuthService(
            UserRepository userRepository,
            PasswordEncoder passwordEncoder,
            JwtService jwtService,
            @org.springframework.beans.factory.annotation.Value("${aquagarden.jwt.expiration-minutes:30}") int expirationMinutes
    ) {
        this.userRepository = userRepository;
        this.passwordEncoder = passwordEncoder;
        this.jwtService = jwtService;
        this.expirationMinutes = expirationMinutes;
    }

    public Map<String, Object> register(RegisterRequest req) {
        if (userRepository.existsByUsername(req.username())) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "用户名已存在");
        }
        String email = normalizeEmail(req.email());
        if (email != null && userRepository.existsByEmail(email)) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "邮箱已被注册");
        }
        User u = new User();
        u.setUsername(req.username());
        u.setEmail(email);
        u.setHashedPassword(passwordEncoder.encode(req.password()));
        u.setRole("user");
        userRepository.save(u);
        return Map.of(
                "success", true,
                "message", "注册成功",
                "user", Map.of(
                        "username", u.getUsername(),
                        "email", u.getEmail() != null ? u.getEmail() : "",
                        "role", u.getRole()
                )
        );
    }

    private static String normalizeEmail(String email) {
        if (email == null) {
            return null;
        }
        String t = email.trim();
        if (t.isEmpty()) {
            return null;
        }
        if (!EMAIL.matcher(t).matches()) {
            throw new ResponseStatusException(HttpStatus.BAD_REQUEST, "邮箱格式不正确");
        }
        return t;
    }

    public TokenResponse login(LoginRequest req) {
        User user = userRepository.findByUsername(req.username())
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.UNAUTHORIZED, "用户名或密码错误"));
        if (!passwordEncoder.matches(req.password(), user.getHashedPassword())) {
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "用户名或密码错误");
        }
        String token = jwtService.generateToken(user.getUsername());
        return new TokenResponse(token, "bearer", expirationMinutes * 60);
    }
}
