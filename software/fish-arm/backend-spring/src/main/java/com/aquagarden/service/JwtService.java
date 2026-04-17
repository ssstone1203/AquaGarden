package com.aquagarden.service;

import io.jsonwebtoken.Claims;
import io.jsonwebtoken.Jwts;
import io.jsonwebtoken.security.Keys;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import javax.crypto.SecretKey;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Date;

@Service
public class JwtService {

    private final SecretKey key;
    private final int expirationMinutes;

    public JwtService(
            @Value("${aquagarden.jwt.secret}") String secret,
            @Value("${aquagarden.jwt.expiration-minutes:30}") int expirationMinutes
    ) {
        this.key = Keys.hmacShaKeyFor(padSecret(secret).getBytes(StandardCharsets.UTF_8));
        this.expirationMinutes = expirationMinutes;
    }

    private static String padSecret(String secret) {
        if (secret.length() >= 32) {
            return secret;
        }
        return secret + "0".repeat(32 - secret.length());
    }

    public String generateToken(String username) {
        Instant now = Instant.now();
        Instant exp = now.plusSeconds(expirationMinutes * 60L);
        return Jwts.builder()
                .subject(username)
                .issuedAt(Date.from(now))
                .expiration(Date.from(exp))
                .signWith(key)
                .compact();
    }

    public String extractUsername(String token) {
        return parse(token).getSubject();
    }

    public boolean isValid(String token) {
        try {
            parse(token);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private Claims parse(String token) {
        return Jwts.parser()
                .verifyWith(key)
                .build()
                .parseSignedClaims(token)
                .getPayload();
    }
}
