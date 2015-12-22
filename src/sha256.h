class Sha256 {
private:
	Sha256& operator+=(const Sha256& rhs){
		for (uint32_t i = 0; i < 8; i++){
			words[i] += rhs.words[i];
		}
	}
	
	friend Sha256 operator+(Sha256 lhs, const Sha256& rhs){
		lhs += rhs;
		return lhs;
	}
public:
	uint32_t words[8];
	
	static Sha256 hash(const uint8_t *data, size_t length);
};
