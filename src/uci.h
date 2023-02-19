class Uci
{
public:
	Uci();

	Uci(Uci&&) = default;

	Uci(const Uci&) = default;

	Uci& operator=(Uci&&) = default;

	Uci& operator=(const Uci&) = default;

	~Uci();

private:
};

Uci::Uci()
{
}

Uci::~Uci()
{
}