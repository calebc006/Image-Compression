class Decimal:

    def __init__(self, value: float = 0, precision: int = 32, verbose=False):
        """
        Implements an arbitrary precision decimal [0, 1)
        """
        self.precision = precision  # number of bits used in binary representation

        from numpy import zeros
        self.bits = zeros(precision, dtype=bool)

        cur_power = -1
        cur_value = value
        for _ in range(precision):
            if cur_value > pow(2, cur_power):
                self.bits[-cur_power - 1] = 1
                cur_value -= pow(2, cur_power)
            cur_power -= 1

        if verbose:
            print(f"Initialized {precision}-bit decimal from base-10 representation of {value}: \n{str(self)}")

    def set_precision(self, precision: int):
        if precision < self.precision:
            self.bits = self.bits[:precision]
        else:
            from numpy import concatenate, zeros
            self.bits = concatenate([self.bits, zeros(precision - self.precision, dtype=bool)])
        self.precision = precision
    
    def to_float(self):
        value = float(0)
        cur_power = -1
        for bit in self.bits:
            if bit:
                value += pow(2, cur_power)
            cur_power -= 1

            if cur_power < -64:
                break

        return value
    
    def __str__(self):
        return ''.join(['0' if i == False else '1' for i in self.bits])
    
    def __add__(self, other):
        pass

    def __sub__(self, other):
        pass

    def __mul__(self, other):
        pass

    def __eq__(self, other):
        return all(b1 == b2 for b1, b2 in zip(self.bits, other.bits))
    
    def __ne__(self, value):
        return not self.__eq__(value)

    def __lt__(self, other):
        pass

    def __gt__(self, other):
        pass



import math
z = Decimal(math.pi/10, precision=128, verbose=True)
print(z.to_float())