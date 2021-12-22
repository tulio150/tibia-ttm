#pragma once

class Aes256 {
	static inline void expand_enc_key(unsigned char *rkey, unsigned char *rc);
	static inline void expand_dec_key(unsigned char *rkey, unsigned char *rc);

	static inline void sub_bytes(unsigned char *buffer);
	static inline void sub_bytes_inv(unsigned char *buffer);

	static inline void copy_key(const unsigned char *key, unsigned char* rkey);

	static inline void add_round_key(unsigned char *rkey, unsigned char *buffer, const unsigned char round);

	static inline void shift_rows(unsigned char *buffer);
	static inline void shift_rows_inv(unsigned char *buffer);

	static inline void mix_columns(unsigned char *buffer);
	static inline void mix_columns_inv(unsigned char *buffer);

public:
	static unsigned long encrypt(const unsigned char *key, unsigned char* plain, unsigned long plain_length);
	static unsigned long decrypt(const unsigned char *key, unsigned char* encrypted, unsigned long encrypted_length);
};
